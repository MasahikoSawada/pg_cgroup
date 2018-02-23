/* pg_cgroup */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_cgroup" to load this file. \quit

CREATE FUNCTION create_resource_group(text, text)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION delete_resource_group(text, text)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION set_resource_value(text, text, text, bigint)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION attach_resource_group(text, text)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT VOLATILE;

CREATE FUNCTION attach_resource_group(text, text, int)
RETURNS bool
AS 'MODULE_PATHNAME', 'attach_resource_group_pid'
LANGUAGE C STRICT VOLATILE;
