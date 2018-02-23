/* -------------------------------------------------------------------------
 *
 * pg_cgroup.c
 *
 * A PostgreSQL extension module to manage cgroups(Control Group).
 *
 * -------------------------------------------------------------------------
 */

#include "postgres.h"

#include <stdlib.h>
#include <libcgroup.h>

#include "commands/extension.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"


PG_MODULE_MAGIC;

/* pg_cgroup.cgroup_name is defined? */
#define isCgroupNameDefined() \
	(cgroup_home_name != NULL && cgroup_home_name[0] != '\0')

/* Get group total name length; "home_len" + "/" + "len"  + '\0' */
#define	getCgroupTotalLen(cgname) \
	(strlen(cgroup_home_name) + 1 + strlen(cgname) + 1)

/* Get cgroup total name string */
#define getCgroupTotalName(buf, g) \
{ \
	snprintf((buf), getCgroupTotalLen((g)), "%s/%s", cgroup_home_name, (g)); \
} while(0) 

/* SQL-callable functions */
PG_FUNCTION_INFO_V1(create_resource_group);
PG_FUNCTION_INFO_V1(delete_resource_group);
PG_FUNCTION_INFO_V1(set_resource_value);
PG_FUNCTION_INFO_V1(attach_resource_group);
PG_FUNCTION_INFO_V1(attach_resource_group_pid);

void	_PG_init(void);

static bool cgroup_inited = false;

/* GUC parameter */
static char *cgroup_home_name = NULL;

static struct cgroup *get_cgroup(char *cgroup_name);
static struct cgroup_controller *get_cgroup_ctl(struct cgroup *cg, char *cgroup_name,
												char *subsys_name, bool missing_ok);

void
_PG_init(void)
{
	DefineCustomStringVariable("pg_cgroup.cgroup_name",
							   "PostgreSQL cgroups name",
							   NULL,
							   &cgroup_home_name,
							   NULL,
							   PGC_SIGHUP,
							   0,
							   NULL,
							   NULL,
							   NULL);
}

/*
 * Return newed cgroup struct by a given name. 'cgroup_name' must be
 * full cgroup name.
 */
static struct cgroup *
get_cgroup(char *cgroup_name)
{
	struct cgroup *cg;

	Assert(cgroup_name != NULL);

	if (!cgroup_inited)
	{
		cgroup_init();
		cgroup_inited = true;
	}

	/* New cgroup struct */
	cg = cgroup_new_cgroup(cgroup_name);
	if (cg == NULL)
		ereport(ERROR, (errmsg("could not new cgroup struct")));

	cgroup_get_cgroup(cg);

	return cg;
}

/*
 * Get cgroup controller struct. This function also add subsystem
 * to a given cgroup. Raise an error when couldn't find a given name
 * cgroup controller if 'missing_ok' is false.
 */
static struct cgroup_controller *
get_cgroup_ctl(struct cgroup *cg, char *cgroup_name, char *subsys_name,
			   bool missing_ok)
{
	struct cgroup_controller *cg_ctl;
	
	Assert(cg != NULL);
	Assert(subsys_name != NULL);

	if (!cgroup_inited)
	{
		cgroup_init();
		cgroup_inited = true;
	}

	cg_ctl = cgroup_get_controller(cg, subsys_name);

	if (cg_ctl == NULL)
	{
		if (!missing_ok)
			ereport(ERROR, (errmsg("could not get cgroup controller: \"%s\", \"%s\"",
								   cgroup_name, subsys_name)));

		/* There is no given subsystem, create it */
		cg_ctl = cgroup_add_controller(cg, subsys_name);
		if (cg_ctl == NULL)
			ereport(ERROR, (errmsg("could not create cgroup controller")));
	}

	return cg_ctl;
}

/* Create new resource group */
Datum
create_resource_group(PG_FUNCTION_ARGS)
{
	char	*subgroup_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char	*subsys_name = text_to_cstring(PG_GETARG_TEXT_PP(1));
	char	*group_full_name;
	struct cgroup *cg;
	struct cgroup_controller *cg_ctl;

	/* Sanity check */
	if (!isCgroupNameDefined())
		ereport(ERROR, (errmsg("pg_cgroup.group_name is empty")));

	/* Get full cgroup name */
	group_full_name = palloc(sizeof(char) * getCgroupTotalLen(subgroup_name));
	getCgroupTotalName(group_full_name, subgroup_name);

	/* New cgroup struct */
	cg = get_cgroup(group_full_name);

	/*
	 * Get cgroup controller of given subsystem. Create new cgroup if not exists.
	 */
	cg_ctl = get_cgroup_ctl(cg, group_full_name, subsys_name, true);

	/* Create new cgroup subsystem actually */
	if (cgroup_create_cgroup(cg, 0) != 0)
		ereport(ERROR, (errmsg("could not create new cgroup: \"%s\", \"%s\"",
							   group_full_name, subsys_name)));

	free((void *) cg);
	free(cg_ctl);

	PG_RETURN_BOOL(true);
}

/* Delete new resource group */
Datum
delete_resource_group(PG_FUNCTION_ARGS)
{
	char	*subgroup_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char	*subsys_name = text_to_cstring(PG_GETARG_TEXT_PP(1));
	char	*group_full_name;
	struct cgroup *cg;
	struct cgroup_controller *cg_ctl;

	/* Sanity check */
	if (!isCgroupNameDefined())
		ereport(ERROR, (errmsg("pg_cgroup.group_name is empty")));

	/* Get full cgroup name */
	group_full_name = palloc(sizeof(char) * getCgroupTotalLen(subgroup_name));
	getCgroupTotalName(group_full_name, subgroup_name);

	/* New cgroup struct */
	cg = get_cgroup(group_full_name);

	/* Get cgroup controller of given subsystem */
	cg_ctl = get_cgroup_ctl(cg, group_full_name, subsys_name, false);

	/* Delete cgroup subsystem actually */
	cgroup_delete_cgroup(cg, 0);

	free((void *)cg);
	free(cg_ctl);

	PG_RETURN_BOOL(true);
}

/* Set parameter to a subsystem */
Datum
set_resource_value(PG_FUNCTION_ARGS)
{
	char	*subgroup_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char	*subsys_name = text_to_cstring(PG_GETARG_TEXT_PP(1));
	char	*param_name = text_to_cstring(PG_GETARG_TEXT_PP(2));
	uint64	value = PG_GETARG_INT64(3);
	char	*group_full_name;
	struct cgroup *cg;
	struct cgroup_controller *cg_ctl;

	/* Sanity check */
	if (!isCgroupNameDefined())
		ereport(ERROR, (errmsg("pg_cgroup.group_name is empty")));

	/* Get full cgroup name */
	group_full_name = palloc(sizeof(char) * getCgroupTotalLen(subgroup_name));
	getCgroupTotalName(group_full_name, subgroup_name);

	/* New cgroup struct */
	cg = get_cgroup(group_full_name);

	/* Get cgroup controller of given subsystem */
	cg_ctl = get_cgroup_ctl(cg, group_full_name, subsys_name, false);

	/* Set value */
	cgroup_set_value_uint64(cg_ctl, param_name, value);

	/* Reflect the modification */
	if (cgroup_modify_cgroup(cg) != 0)
		ereport(ERROR, (errmsg("could not modify cgroup : \"%s\", subsyste \"%s\", param \"%s\" and value %lu",
							   group_full_name, subsys_name,
							   param_name, value)));

	free(cg);
	free(cg_ctl);

	PG_RETURN_BOOL(true);
}

/* Attach myself to a cgroup */
Datum
attach_resource_group(PG_FUNCTION_ARGS)
{
	char	*subgroup_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char	*subsys_name = text_to_cstring(PG_GETARG_TEXT_PP(1));
	char	*group_full_name;
	struct cgroup *cg;

	/* Sanity check */
	if (!isCgroupNameDefined())
		ereport(ERROR, (errmsg("pg_cgroup.group_name is empty")));

	/* Get full cgroup name */
	group_full_name = palloc(sizeof(char) * getCgroupTotalLen(subgroup_name));
	getCgroupTotalName(group_full_name, subgroup_name);

	/* New cgroup struct */
	cg = get_cgroup(group_full_name);

	/* Get cgroup controller of given subsystem */
	get_cgroup_ctl(cg, group_full_name, subsys_name, false);

	/* Attache myself to given cgroup */
	cgroup_attach_task(cg);

	PG_RETURN_BOOL(true);
}

/* Attach given pid to a cgroup */
Datum
attach_resource_group_pid(PG_FUNCTION_ARGS)
{
	char	*subgroup_name = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char	*subsys_name = text_to_cstring(PG_GETARG_TEXT_PP(1));
	pid_t	pid = (pid_t) PG_GETARG_INT32(2);
	char	*group_full_name;
	struct cgroup *cg;

	/* Sanity check */
	if (!isCgroupNameDefined())
		ereport(ERROR, (errmsg("pg_cgroup.group_name is empty")));

	/* Get full cgroup name */
	group_full_name = palloc(sizeof(char) * getCgroupTotalLen(subgroup_name));
	getCgroupTotalName(group_full_name, subgroup_name);

	/* New cgroup struct */
	cg = get_cgroup(group_full_name);

	/* Get cgroup controller of given subsystem */
	get_cgroup_ctl(cg, group_full_name, subsys_name, false);

	/* Attache a given pid to given cgroup */
	cgroup_attach_task_pid(cg, pid);

	PG_RETURN_BOOL(true);
}
