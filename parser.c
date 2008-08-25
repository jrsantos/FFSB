/*
 *   Copyright (c) International Business Machines Corp., 2001-2004
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <ctype.h>

#include "ffsb.h"
#include "parser.h"
#include "ffsb_tg.h"
#include "ffsb_stats.h"
#include "util.h"
#include "list.h"

#define BUFSIZE 1024

/* strips out whitespace and comments, returns NULL on eof */
void parseerror(char *msg)
{
	fprintf(stderr, "Error parsing %s\n", msg);
	exit(1);
}

static char *get_next_line(FILE *f)
{
	static char buf[BUFSIZE];
	char *ret;
	int flag = 1;
	while (flag) {
		ret = fgets(buf, BUFSIZE, f);
		if (ret == NULL)
			return NULL;
		ret = buf;
		while (isspace(*ret))
			ret++;

		if ((*ret == COMMENT_CHAR) || (*ret == '\0'))
			continue;
		flag = 0;
	}
	return ret;
}

static uint64_t *get_opt64(char *buf, char string[])
{
	char search_str[256];
	uint64_t temp;
	uint64_t *ret;

	sprintf(search_str, "%s=%%llu\\n", string);
	if (1 == sscanf(buf, search_str, &temp)) {
		ret = malloc(sizeof(uint64_t));
		*ret = temp;
		return ret;
	}
	return NULL;
}

static uint32_t *get_opt32(char *buf, char string[])
{
	uint32_t *ret;
	uint64_t *res;
	res = get_opt64(buf, string);
	if (res) {
		ret = malloc(sizeof(uint32_t));
		*ret = *res;
		free(res);
		return ret;
	}
	return NULL;
}

static uint8_t *get_optbool(char *buf, char string[])
{
	uint8_t *ret;
	uint64_t *res;
	res = get_opt64(buf, string);
	if (res) {
		if ((int)*res < 0 || (int)*res > 1) {
			printf("Error in: %s", buf);
			printf("%llu not boolean\n", (long long unsigned) *res);
			exit(1);
		}
		ret = malloc(sizeof(uint8_t));
		*ret = *res;
		free(res);
		return ret;
	}
	return NULL;
}

static char *get_optstr(char *buf, char string[])
{
	char search_str[256];
	char *ret_buf;
	char temp[BUFSIZE];
	int len;

	len = strnlen(string, BUFSIZE);
	sprintf(search_str, "%s=%%%ds\\n", string, BUFSIZE - len-1);
	if (1 == sscanf(buf, search_str, &temp)) {
		len = strnlen(temp, 4096);
		ret_buf = malloc(len);
		strncpy(ret_buf, temp, len);
		return ret_buf;
		}
	return NULL;
}

static double *get_optdouble(char *buf, char string[])
{
	char search_str[256];
	double temp;
	double *ret;

	sprintf(search_str, "%s=%%lf\\n", string);
	if (1 == sscanf(buf, search_str, &temp)) {
		ret = malloc(sizeof(double));
		*ret = temp;
		return ret;
	}
	return NULL;
}

static range_t *get_optrange(char *buf, char string[])
{
	char search_str[256];
	double a, b;
	range_t *ret;

	sprintf(search_str, "%s %%lf %%lf\\n", string);
	if (2 == sscanf(buf, search_str, &a, &b)) {
		ret = malloc(sizeof(struct range));
		ret->a = a;
		ret->b = b;
		return ret;
	}
	return NULL;
}

static size_weight_t *get_optsizeweight(char *buf, char string[])
{
	char search_str[256];
	uint64_t size;
	int weight;
	size_weight_t *ret;

	sprintf(search_str, "%s %%llu %%d\\n", string);
	if (2 == sscanf(buf, search_str, &size, &weight)) {
		ret = malloc(sizeof(struct size_weight));
		ret->size = size;
		ret->weight = weight;
		return ret;
	}
	return NULL;
}

static uint64_t *get_deprecated(char *buf, char string[])
{
	char search_str[256];
	char temp[BUFSIZE];
	int len;

	len = strnlen(string, BUFSIZE);
	sprintf(search_str, "%s%%%ds\\n", string, BUFSIZE - len-1);
	if (1 == sscanf(buf, search_str, &temp))
		printf("WARNING: The \"%s\" option is deprecated!!!\n", string);

	return NULL;
}

config_options_t global_options[] = {
	{"num_filesystems", NULL, TYPE_DEPRECATED, STORE_SINGLE},
	{"num_threadgroups", NULL, TYPE_DEPRECATED, STORE_SINGLE},
	{"verbose", NULL, TYPE_BOOLEAN, STORE_SINGLE},
	{"time", NULL, TYPE_U32, STORE_SINGLE},
	{"directio", NULL, TYPE_BOOLEAN, STORE_SINGLE},
	{"bufferio", NULL, TYPE_BOOLEAN, STORE_SINGLE},
	{"alignio", NULL, TYPE_BOOLEAN, STORE_SINGLE},
	{"callout", NULL, TYPE_STRING, STORE_SINGLE},
	{NULL, NULL, 0, 0} };

config_options_t tg_options[] = {
	{"bindfs", NULL, TYPE_U32, STORE_SINGLE},
	{"num_threads", NULL, TYPE_U32, STORE_SINGLE},
	{"read_weight", NULL, TYPE_U32, STORE_SINGLE},
	{"readall_weight", NULL, TYPE_U32, STORE_SINGLE},
	{"read_random", NULL, TYPE_U32, STORE_SINGLE},
	{"read_skip", NULL, TYPE_U32, STORE_SINGLE},
	{"read_size", NULL, TYPE_U64, STORE_SINGLE},
	{"read_blocksize", NULL, TYPE_U32, STORE_SINGLE},
	{"read_skipsize", NULL, TYPE_U32, STORE_SINGLE},
	{"write_weight", NULL, TYPE_U32, STORE_SINGLE},
	{"write_random", NULL, TYPE_U32, STORE_SINGLE},
	{"fsync_file", NULL, TYPE_U32, STORE_SINGLE},
	{"write_size", NULL, TYPE_U64, STORE_SINGLE},
	{"write_blocksize", NULL, TYPE_U32, STORE_SINGLE},
	{"create_weight", NULL, TYPE_U32, STORE_SINGLE},
	{"delete_weight", NULL, TYPE_U32, STORE_SINGLE},
	{"append_weight", NULL, TYPE_U32, STORE_SINGLE},
	{"meta_weight", NULL, TYPE_U32, STORE_SINGLE},
	{"createdir_weight", NULL, TYPE_U32, STORE_SINGLE},
	{"op_delay", NULL, TYPE_U32, STORE_SINGLE},
	{NULL, NULL, 0} };

config_options_t fs_options[] = {
	{"location", NULL, TYPE_STRING, STORE_SINGLE},
	{"num_files", NULL, TYPE_U32, STORE_SINGLE},
	{"num_dirs", NULL, TYPE_U32, STORE_SINGLE},
	{"reuse", NULL, TYPE_BOOLEAN, STORE_SINGLE},
	{"min_filesize", NULL, TYPE_U64, STORE_SINGLE},
	{"max_filesize", NULL, TYPE_U64, STORE_SINGLE},
	{"create_blocksize", NULL, TYPE_U32, STORE_SINGLE},
	{"age_blocksize", NULL, TYPE_U32, STORE_SINGLE},
	{"desired_util", NULL, TYPE_DOUBLE, STORE_SINGLE},
	{"agefs", NULL, TYPE_BOOLEAN, STORE_SINGLE},
	{"size_weight", NULL, TYPE_SIZEWEIGHT, STORE_LIST},
	{"init_util", NULL, TYPE_DOUBLE, STORE_SINGLE},
	{"init_size", NULL, TYPE_U64, STORE_SINGLE},
	{NULL, NULL, 0} };

config_options_t stats_options[] = {
	{"enable_stats", NULL, TYPE_BOOLEAN, STORE_SINGLE},
	{"ignore", NULL, TYPE_STRING, STORE_LIST},
	{"bucket", NULL, TYPE_RANGE, STORE_LIST},
	{NULL, NULL, 0} };

container_desc_t container_desc[] = {
	{"filesystem", FILESYSTEM, 10},
	{"threadgroup", THREAD_GROUP, 11},
	{"end", END, 3},
	{"stats", STATS, 5},
	{NULL, 0, 0} };

static container_t *init_container(void)
{
	container_t *container;
	container = malloc(sizeof(container_t));
	container->config = NULL;
	container->type = 0;
	container->next = NULL;
	return container;
}

static int set_option(char *buf, config_options_t *options)
{
	void *value;

	while (options->name) {
		switch (options->type) {
		case TYPE_U32:
			value = get_opt32(buf, options->name);
			if (value)
				goto out;
			break;
		case TYPE_U64:
			value = get_opt64(buf, options->name);
			if (value)
				goto out;
			break;
		case TYPE_STRING:
			value = get_optstr(buf, options->name);
			if (value)
				goto out;
			break;
		case TYPE_BOOLEAN:
			value = get_optbool(buf, options->name);
			if (value)
				goto out;
			break;
		case TYPE_DOUBLE:
			value = get_optdouble(buf, options->name);
			if (value)
				goto out;
			break;
		case TYPE_RANGE:
			value = get_optrange(buf, options->name);
			if (value)
				goto out;
			break;
		case TYPE_SIZEWEIGHT:
			value = get_optsizeweight(buf, options->name);
			if (value)
				goto out;
			break;
		case TYPE_DEPRECATED:
			value = get_deprecated(buf, options->name);
			if (value)
				goto out;
			break;
		default:
			printf("Unknown type\n");
			break;
		}
		options++;
	}
	return 0;

out:
	if (options->storage_type == STORE_SINGLE)
		options->value = value;
	if (options->storage_type == STORE_LIST) {
		if (!options->value) {
			value_list_t *lhead;
			lhead = malloc(sizeof(struct value_list));
			INIT_LIST_HEAD(&lhead->list);
			options->value = lhead;
		}
		value_list_t *tmp_list, *tmp_list2;
		tmp_list = malloc(sizeof(struct value_list));
		INIT_LIST_HEAD(&tmp_list->list);
		tmp_list->value = value;
		tmp_list2 = (struct value_list *)options->value;
		list_add(&(tmp_list->list), &(tmp_list2->list));
	}

	return 1;
}

void insert_container(container_t *container, container_t *new_container)
{
	while (container->next)
		container = container->next;
	container->next = new_container;
}

container_t *search_group(char *, FILE *);

container_t *handle_container(char *buf, FILE *f, uint32_t type,
			      config_options_t *options)
{
	container_desc_t *desc = container_desc;
	container_t *ret_container;
	container_t *tmp_container, *tmp2_container;
	container_t *child = NULL;

	while (desc->name)
		if (desc->type == type)
			break;
		else
			desc++;

	if (!desc->name)
		return NULL;

	buf = get_next_line(f);
	while (buf) {
		set_option(buf, options);
		tmp_container = search_group(buf, f);
		if (tmp_container) {
			if (tmp_container->type == END) {
				free(tmp_container);
				break;
			} else {
				if (child == NULL)
					child = tmp_container;
				else {
					tmp2_container = child;
					while (tmp2_container->next)
						tmp2_container = tmp2_container->next;
					tmp2_container->next = tmp_container;
				}

			}
		}
		buf = get_next_line(f);
	}
	ret_container = init_container();
	ret_container->config = options;
	ret_container->type = type;
	if (child)
		ret_container->child = child;

	return ret_container;
}

container_t *search_group(char *buf, FILE *f)
{
	char temp[BUFSIZE];
	char *ptr;
	config_options_t *options;
	container_desc_t *desc = container_desc;
	container_t *ret_container;

	if (1 == sscanf(buf, "[%s]\n", (char *) &temp))
		while (desc->name) {
			ptr = strstr(buf, desc->name);
			if (ptr)
				switch (desc->type) {
				case FILESYSTEM:
					options = malloc(sizeof(fs_options));
					memcpy(options, fs_options,
					       sizeof(fs_options));
					return handle_container(buf, f,
								desc->type,
								options);
					break;
				case THREAD_GROUP:
					options = malloc(sizeof(tg_options));
					memcpy(options, tg_options,
					       sizeof(tg_options));
					return handle_container(buf, f,
								desc->type,
								options);
					break;
				case STATS:
					options = malloc(sizeof(stats_options));
					memcpy(options, stats_options,
					       sizeof(stats_options));
					return handle_container(buf, f,
								desc->type,
								options);
					break;
				case END:
					ret_container = init_container();
					ret_container->type = END;
					return ret_container;
					break;
				}
			desc++;
		}
	return NULL;
}

void *get_value(config_options_t *config, char *name)
{
	while (config->name) {
		if (!strcmp(config->name, name)) {
			if (config->value)
				return config->value;
			else
				return NULL;
		}
		config++;
	}
	return 0;
}

char *get_config_str(config_options_t *config, char *name)
{
	return get_value(config, name);
}

uint32_t get_config_u32(config_options_t *config, char *name)
{
	void *value = get_value(config, name);
	if (value)
		return *(uint32_t *)value;
	return 0;
}

uint8_t get_config_bool(config_options_t *config, char *name)
{
	void *value = get_value(config, name);
	if (value)
		return *(uint8_t *)value;
	return 0;
}

uint32_t get_config_u64(config_options_t *config, char *name)
{
	void *value = get_value(config, name);
	if (value)
		return *(uint64_t *)value;
	return 0;
}

double get_config_double(config_options_t *config, char *name)
{
	void *value = get_value(config, name);
	if (value)
		return *(double *)value;
	return 0;
}

static profile_config_t *parse(FILE *f)
{
	char *buf;
	profile_config_t *profile_conf;
	container_t *tmp_container;

	profile_conf = malloc(sizeof(profile_config_t));
	profile_conf->global = malloc(sizeof(global_options));
	memcpy(profile_conf->global, global_options, sizeof(global_options));
	profile_conf->fs_container = NULL;
	profile_conf->tg_container = NULL;

	buf = get_next_line(f);

	while (buf) {
		set_option(buf, profile_conf->global);
		tmp_container = search_group(buf, f);
		if (tmp_container)
			switch (tmp_container->type) {
			case FILESYSTEM:
				if (profile_conf->fs_container == NULL)
					profile_conf->fs_container = tmp_container;
				else
					insert_container(profile_conf->fs_container,
							 tmp_container);
				break;
			case THREAD_GROUP:
				if (profile_conf->tg_container == NULL)
					profile_conf->tg_container = tmp_container;
				else
					insert_container(profile_conf->tg_container,
							 tmp_container);
				break;
			default:
				break;
			}
		buf = get_next_line(f);
	}
	return profile_conf;
}

/* !!! hackish verification function, we should somehow roll this into the */
/* op descriptions/struct themselves at some point with a callback verify */
/* op requirements: */
/* require tg->read_blocksize:  read, readall */
/* require tg->write_blocksize: write, create, append, rewritefsync */
/* */

static int verify_tg(ffsb_tg_t *tg)
{
	uint32_t read_weight    = tg_get_op_weight(tg, "read");
	uint32_t readall_weight = tg_get_op_weight(tg, "readall");
	uint32_t write_weight   = tg_get_op_weight(tg, "write");
	uint32_t create_weight  = tg_get_op_weight(tg, "create");
	uint32_t append_weight  = tg_get_op_weight(tg, "append");
	uint32_t metaop_weight    = tg_get_op_weight(tg, "metaop");
	uint32_t createdir_weight = tg_get_op_weight(tg, "createdir");
	uint32_t delete_weight    = tg_get_op_weight(tg, "delete");

	uint32_t sum_weight = read_weight +
		readall_weight +
		write_weight +
		create_weight +
		append_weight +
		metaop_weight +
		createdir_weight +
		delete_weight;


	uint32_t read_blocksize  = tg_get_read_blocksize(tg);
	uint32_t write_blocksize = tg_get_write_blocksize(tg);

	int read_random          = tg_get_read_random(tg);
	int read_skip            = tg_get_read_skip(tg);
	uint32_t read_skipsize   = tg_get_read_skipsize(tg);

	if (sum_weight == 0) {
		printf("Error: A threadgroup must have at least one weighted "
		       "operation\n");
		return 1;
	}

	if ((read_weight || readall_weight) && !(read_blocksize)) {
		printf("Error: read and readall operations require a "
		       "read_blocksize\n");
		return 1;
	}

	if ((write_weight || create_weight || append_weight) &&
	     !(write_blocksize)) {
		printf("Error: write, create, append"
		       "operations require a write_blocksize\n");
		return 1;
	}

	if (read_random && read_skip) {
		printf("Error: read_random and read_skip are mutually "
		       "exclusive\n");
		return 1;
	}

	if (read_skip && !(read_skipsize)) {
		printf("Error: read_skip specified but read_skipsize is "
		       "zero\n");
		return 1;
	}

	return 0;
}

static unsigned get_num_containers(container_t *container)
{
	int numtg = 0;
	while (container) {
		numtg++;
		container = container->next;
	}
	return numtg;
}

static unsigned get_num_threadgroups(profile_config_t *profile_conf)
{
	return get_num_containers(profile_conf->tg_container);
}

static unsigned get_num_filesystems(profile_config_t *profile_conf)
{
	return get_num_containers(profile_conf->fs_container);
}

static int get_num_totalthreads(profile_config_t *profile_conf)
{
	int num_threads = 0;
	container_t *tg = profile_conf->tg_container;
	config_options_t *tg_config;

	while (tg) {
		tg_config = tg->config;
		while (tg_config->name) {
			if (!strcmp(tg_config->name, "num_threads"))
				num_threads += *(uint32_t *) tg_config->value;
			tg_config++;
		}
		if (tg->next)
			tg = tg->next;
		else
			break;
	}

	return num_threads;
}

container_t *get_container(container_t *head_cont, int pos)
{
	int count = 0;
	while (head_cont) {
		if (count == pos)
			return head_cont;
		head_cont = head_cont->next;
		count++;
	}
	return NULL;
}

config_options_t *get_fs_config(ffsb_config_t *fc, int pos)
{
	container_t *tmp_cont;

	assert(pos < fc->num_filesys);
	tmp_cont = get_container(fc->profile_conf->fs_container, pos);
	if (tmp_cont)
		return tmp_cont->config;
	return NULL;
}

container_t *get_fs_container(ffsb_config_t *fc, int pos)
{
	assert(pos < fc->num_filesys);
	return get_container(fc->profile_conf->fs_container, pos);
}

config_options_t *get_tg_config(ffsb_config_t *fc, int pos)
{
	container_t *tmp_cont;

	assert(pos < fc->num_filesys);
	tmp_cont = get_container(fc->profile_conf->tg_container, pos);
	if (tmp_cont)
		return tmp_cont->config;
	return NULL;
}

container_t *get_tg_container(ffsb_config_t *fc, int pos)
{
	assert(pos < fc->num_filesys);
	return get_container(fc->profile_conf->tg_container, pos);
}

static void init_threadgroup(config_options_t *config,
			     ffsb_tg_t *tg, int tg_num)
{
	int num_threads;

	memset(tg, 0, sizeof(ffsb_tg_t));

	num_threads = get_config_u32(config, "num_threads");

	init_ffsb_tg(tg, num_threads, tg_num);

	tg->bindfs = get_config_bool(config, "bindfs");

	tg->read_random = get_config_bool(config, "read_random");
	tg->read_size = get_config_u64(config, "read_size");
	tg->read_blocksize = get_config_u32(config, "read_blocksize");
	tg->read_skip = get_config_bool(config, "read_skip");
	tg->read_skipsize = get_config_u32(config, "read_skipsize");

	tg->write_random = get_config_bool(config, "write_random");
	tg->write_size = get_config_u64(config, "write_size");
	tg->write_blocksize = get_config_u32(config, "write_blocksize");
	tg->fsync_file = get_config_bool(config, "fsync_file");

	tg->wait_time = get_config_u32(config, "op_delay");

	tg_set_op_weight(tg, "read", get_config_u32(config, "read_weight"));
	tg_set_op_weight(tg, "readall", get_config_u32(config, "readall_weight"));
	tg_set_op_weight(tg, "write", get_config_u32(config, "write_weight"));
	tg_set_op_weight(tg, "append", get_config_u32(config, "append_weight"));
	tg_set_op_weight(tg, "create", get_config_u32(config, "create_weight"));
	tg_set_op_weight(tg, "delete", get_config_u32(config, "delete_weight"));
	tg_set_op_weight(tg, "metaop", get_config_u32(config, "meta_weight"));
	tg_set_op_weight(tg, "createdir", get_config_u32(config, "createdir_weight"));
	if (verify_tg(tg)) {
		printf("threadgroup %d verification failed\n", tg_num);
		exit(1);
	}
}

static void init_filesys(ffsb_config_t *fc, int num)
{
	config_options_t *config = get_fs_config(fc, num);
	profile_config_t *profile_conf = fc->profile_conf;
	ffsb_fs_t *fs = &fc->filesystems[num];
	value_list_t *tmp_list, *list_head;
	int *i, j;

	memset(fs, 0, sizeof(ffsb_fs_t));

	fs->basedir = get_config_str(config, "location");
	fs->num_dirs = get_config_u32(config, "num_dirs");
	fs->num_start_files = get_config_u32(config, "num_files");
	fs->minfilesize = get_config_u64(config, "min_filesize");
	fs->maxfilesize = get_config_u64(config, "max_filesize");
	fs->desired_fsutil = get_config_double(config, "desired_util");
	fs->init_fsutil = get_config_double(config, "init_util");
	fs->init_size = get_config_u64(config, "init_size");

	fs->flags = 0;
	if (get_config_bool(config, "reuse"))
		fs->flags |= FFSB_FS_REUSE_FS;

	if (get_config_bool(profile_conf->global, "directio"))
		fs->flags |= FFSB_FS_DIRECTIO | FFSB_FS_ALIGNIO4K;

	if (get_config_bool(profile_conf->global, "bufferio"))
		fs->flags |= FFSB_FS_LIBCIO;

	if (get_config_bool(profile_conf->global, "alignio"))
		fs->flags |= FFSB_FS_ALIGNIO4K;

	if (get_config_bool(config, "agefs")) {
		container_t *age_cont = get_fs_container(fc, num);
		if (!age_cont->child) {
			printf("No age threaggroup in profile");
			exit(1);
		}

		age_cont = age_cont->child;
		ffsb_tg_t *age_tg = ffsb_malloc(sizeof(ffsb_tg_t));
		init_threadgroup(age_cont->config, age_tg, 0);
		fs->aging_tg = age_tg;
		fs->age_fs = 1;
	}

	if (get_config_u32(config, "create_blocksize"))
		fs->create_blocksize = get_config_u32(config,
							"create_blocksize");
	else
		fs->create_blocksize = FFSB_FS_DEFAULT_CREATE_BLOCKSIZE;

	if (get_config_u32(config, "age_blocksize"))
		fs->age_blocksize = get_config_u32(config, "age_blocksize");
	else
		fs->age_blocksize = FFSB_FS_DEFAULT_AGE_BLOCKSIZE;

	list_head = (value_list_t *) get_value(config, "size_weight");
	if (list_head) {
		int count = 0;
		size_weight_t *sizew;
		list_for_each_entry(tmp_list, &list_head->list, list)
			count++;

		fs->num_weights=count;
		fs->size_weights = malloc(sizeof(size_weight_t) * fs->num_weights);

		count = 0;
		list_for_each_entry(tmp_list, &list_head->list, list) {
			sizew = (size_weight_t *)tmp_list->value;
			fs->size_weights[count].size = sizew->size;
			fs->size_weights[count].weight = sizew->weight;
			fs->sum_weights += sizew->weight;
			count++;
		}
	}
}

static void init_tg_stats(ffsb_config_t *fc, int num)
{
	config_options_t *config;
	container_t *tmp_cont;
	value_list_t *tmp_list, *list_head;
	syscall_t sys;
	ffsb_statsc_t fsc = { 0, };
	char *sys_name;
	range_t *bucket_range;
	uint32_t min, max;

	tmp_cont = get_tg_container(fc, num);
	if (tmp_cont->child) {
		tmp_cont = tmp_cont->child;
		if (tmp_cont->type == STATS) {
			config = tmp_cont->config;
			if (get_config_bool(config, "enable_stats")) {

				list_head = (value_list_t *) get_value(config, "ignore");
				if (list_head)
					list_for_each_entry(tmp_list,
							    &list_head->list, list) {
						sys_name = (char *)tmp_list->value;
						ffsb_stats_str2syscall(sys_name, &sys);
						ffsb_statsc_ignore_sys(&fsc, sys);
					}

				list_head = (value_list_t *) get_value(config, "bucket");
				if (list_head)
					list_for_each_entry(tmp_list,
							    &list_head->list, list) {
						bucket_range = (range_t *)tmp_list->value;
						min = (uint32_t)(bucket_range->a * 1000.0f);
						max = (uint32_t)(bucket_range->b * 1000.0f);
						ffsb_statsc_addbucket(&fsc, min, max);
					}

				tg_set_statsc(&fc->groups[num], &fsc);
			}
		}
	}
}

static void init_config(ffsb_config_t *fc, profile_config_t *profile_conf)
{
	config_options_t *config;
	container_t *tmp_cont;
	int i;

	fc->time = get_config_u32(profile_conf->global, "time");
	fc->num_filesys = get_num_filesystems(profile_conf);
	fc->num_threadgroups = get_num_threadgroups(profile_conf);
	fc->num_totalthreads = get_num_totalthreads(profile_conf);
	fc->profile_conf = profile_conf;
	fc->callout = get_config_str(profile_conf->global, "callout");

	fc->filesystems = ffsb_malloc(sizeof(ffsb_fs_t) * fc->num_filesys);
	for (i = 0; i < fc->num_filesys; i++)
		init_filesys(fc, i);

	fc->groups = ffsb_malloc(sizeof(ffsb_tg_t) * fc->num_threadgroups);
	for (i = 0; i < fc->num_threadgroups; i++) {
		config = get_tg_config(fc, i);
		init_threadgroup(config, &fc->groups[i], i);
		init_tg_stats(fc, i);
	}
}

void ffsb_parse_newconfig(ffsb_config_t *fc, char *filename)
{
	FILE *f;

	profile_config_t *profile_conf;

	f = fopen(filename, "r");
	if (f == NULL) {
		perror(filename);
		exit(1);
	}
	profile_conf = parse(f);
	fclose(f);

	init_config(fc, profile_conf);
}
