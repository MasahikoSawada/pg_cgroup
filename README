# pg_cgroup
A PostgreSQL extension to manage cgroup (Control Group).

## Overview
`pg_cgroups` provides set of SQL-callable functions to manage cgroup on Linux OS. `pg_cgroups` requires user to create a cgroup for PostgreSQL server in advance. Using `pg_cgroup` you can both create/delete/set cgroups and classify pids with in the cgroup of PostgreSQL server.

## GUC Parameter
* pg_cgroup.cgroup_name (text)
  * The name of cgroup of which owner is the user who was used for initdb.

## Example
### Installation
You can install `pg_cgroup` extension in similar way as other PostgreSQL extensions except for requiring `libcgroup-devel` package.
1. Get `libcgroup-devel` package
```bash
$ sudo yum -y libcgroup-devel

```
2. Get `pg_cgroup` source code
```bash
$ git clone git@github.com:MasahikoSawada/pg_cgroup.git
```
3. Build and Install
```bash
$ make USE_PGXS=1
$ sudo make USE_PGXS=1 install
```
### Setup
1. Create cgroup for PostgreSQL with definition of user and group
```bash
$ cgcreate -g cpu,memory,disk:pgsql -t postgres -a postgres
```
2. Configuration fo pg_cgroup
```bash
$ vi /path/to/postgresql.conf
shared_preload_libraries = 'pg_cgroup'
pg_cgroup.cgroup_name = 'pgsql'
```
3. Start PostgreSQL Server
4. Create EXTENSION
```bash
$ psql -c 'CREATE EXTENSION pg_cgroup'
```

### Usage
* Create new resource group of cpu controller
```sql
=# SELECT create_resource_group('test_cg', 'cpu');
 create_resource_group
-----------------------
 t
(1 row)
```

Now you can see the created new cgroup by using `lscgroup` or whatever you'd like to use as follows.

```bash
$ lscgroup | grep pgsql
cpu,cpuacct:/pgsql/
cpu,cpuacct:/pgsql/test_cg
```

* Set parameter of cpu cgroup
```sql
=# SELECT set_resource_value('test_cg', 'cpu', 'cpu.cfs_quota_us', 50000);
 set_resource_value
--------------------
 t
(1 row)

```

## Test Platforms
|Category|Module Name|
|--------|-----------|
|OS|CentOS 7.4|
|DBMS|PostgreSQL 10|
|Library|libcgroup 0.41-13|

## Copyright
Copyright © 2018- Masahiko Sawada

## License
The PostgreSQL License. (same as BSD License)

