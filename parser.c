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

#define BUFSIZE 1024

/* strips out whitespace and comments, returns NULL on eof */
void parseerror(char *msg) {
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
	sprintf(search_str, "%s=%%%ds\\n", string, BUFSIZE - len -1);
	if (1 == sscanf(buf, search_str, &temp)) {
		len = strnlen(temp, 4096);
		ret_buf = malloc(len);
		strncpy(ret_buf, temp, len);
		return ret_buf;
		}
	return NULL;
}

struct config_options_t global_options[] = {
	{"num_filesystems", NULL, TYPE_U32},
	{"num_threadgroups", NULL, TYPE_U32},
	{"verbose", NULL, TYPE_BOOLEAN},
	{"time", NULL, TYPE_U32},
	{"directio", NULL, TYPE_BOOLEAN},
	{"bufferio", NULL, TYPE_BOOLEAN},
	{"alignio", NULL, TYPE_BOOLEAN},
	{"callout", NULL, TYPE_STRING},
	{NULL, NULL, 0}};
	
struct config_options_t tg_options[] = {
	{"bindfs", NULL, TYPE_U32},
	{"num_threads", NULL, TYPE_U32},
	{"read_weight", NULL, TYPE_U32},
	{"readall_weight", NULL, TYPE_U32},
	{"read_random", NULL, TYPE_U32},
	{"read_skip", NULL, TYPE_U32},
	{"read_size", NULL, TYPE_U64},
	{"read_blocksize", NULL, TYPE_U32},
	{"read_skipsize", NULL, TYPE_U32},
	{"write_weight", NULL, TYPE_U32},
	{"write_random", NULL, TYPE_U32},
	{"fsync_file", NULL, TYPE_U32},
	{"write_size", NULL, TYPE_U64},
	{"write_blocksize", NULL, TYPE_U32},
	{"create_weight", NULL, TYPE_U32},
	{"delete_weight", NULL, TYPE_U32},
	{"append_weight", NULL, TYPE_U32},
	{"meta_weight", NULL, TYPE_U32},
	{"createdir_weight", NULL, TYPE_U32},
	{"op_delay", NULL, TYPE_U32},
	{NULL, NULL, 0}};
	
struct config_options_t fs_options[] = {
	{"location", NULL, TYPE_STRING},
	{"num_files", NULL, TYPE_U32},
	{"num_dirs", NULL, TYPE_U32},
	{"reuse", NULL, TYPE_BOOLEAN},
	{"min_filesize", NULL, TYPE_U64},
	{"max_filesize", NULL, TYPE_U64},
	{"create_blocksize", NULL, TYPE_U32},
	{"age_blocksize", NULL, TYPE_U32},
	{"desired_util", NULL, TYPE_U32},
	{NULL, NULL, 0}};

struct container_desc_t container_desc[] = {
	{"filesystem", FILESYSTEM, 10},
	{"threadgroup", THREAD_GROUP, 11},
	{"end", END, 3},
	{NULL, 0, 0}};

static struct container_t *init_container(void)
{
	struct container_t *container;
	container = malloc(sizeof(struct container_t));
	container->config = NULL;
	container->type = 0;
	container->next = NULL;
	return container;
}

static int set_option(char *buf, struct config_options_t *options)
{
	while (options->name) {
		switch (options->type) {
		case TYPE_U32:
			if (get_opt32(buf, options->name)) {
				options->value = get_opt32(buf, options->name);
				return 1;
			}
			break;
		case TYPE_U64:
			if (get_opt64(buf, options->name)) {
				options->value = get_opt64(buf, options->name);
				return 1;
			}
			break;
		case TYPE_STRING:
			if (get_optstr(buf, options->name)) {
				options->value = get_optstr(buf, options->name);
				return 1;
			}
			break;
		case TYPE_BOOLEAN:
			if (get_optbool(buf, options->name)) {
				options->value = get_optbool(buf, options->name);
				return 1;
			}
			break;
		default:
			printf("Unknown type\n");
			break;
		}
		options++;
	}
	return 0;
}

void insert_container(struct container_t *container,
		      struct container_t *new_container)
{
	while (container->next)
		container = container->next;
	container->next = new_container;
}

struct container_t *search_group(char *, FILE *);

struct container_t *handle_container(char *buf, FILE *f, uint32_t type,
				     struct config_options_t *options)
{
	struct container_desc_t *desc = container_desc;
	struct container_t *ret_container;
	struct container_t *tmp_container, *tmp2_container;
	struct container_t *child = NULL;
	
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

struct container_t *search_group(char *buf, FILE *f)
{
	char temp[BUFSIZE];
	char *ptr;
	struct config_options_t *options;
	struct container_desc_t *desc = container_desc;
	struct container_t *ret_container;

	if (1 == sscanf(buf, "[%s]\n",(char *) &temp))
		while (desc->name) {
			if ((ptr = strstr(buf, desc->name)))
				switch(desc->type){
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

static void print_value_string(struct config_options_t *config)
{
	switch (config->type) {
		case TYPE_U32:
			printf("%d", *(uint32_t *)config->value);
			break;
		case TYPE_U64:
			printf("%llu", *(uint64_t *)config->value);
			break;
		case TYPE_STRING:
			printf("%s", (char *)config->value);
			break;
		case TYPE_BOOLEAN:
			break;
	}
}

static void print_config(struct config_t *config)
{
	int count = 0;

	struct config_options_t *tmp_config;
	struct container_t *tmp_cont;

	tmp_config = config->global;
	while (tmp_config->name) {
		if (tmp_config->value) {
			printf("\t%s=",tmp_config->name);
			print_value_string(tmp_config);
			printf("\n");
		}
		tmp_config++;
	}

	tmp_cont = config->fs_container;
	while(tmp_cont) {
		tmp_config = tmp_cont->config;
		printf ("Filesystem #%d\n", count);
		while(tmp_config->name) {
			if (tmp_config->value) {
				printf("\t%s=",tmp_config->name);
				print_value_string(tmp_config);
				printf("\n");
			}
			tmp_config++;
		}
		tmp_cont = tmp_cont->next;
		count++;
	}
	printf("\n");
	count = 0;
	tmp_cont = config->tg_container;
	while(tmp_cont) {
		tmp_config = tmp_cont->config;
		printf("Threadgroup #%d\n", count);
		while(tmp_config->name) {
			if (tmp_config->value) {
			printf("\t%s=",tmp_config->name);
				print_value_string(tmp_config);
				printf("\n");
			}
			tmp_config++;
		}
		tmp_cont = tmp_cont->next;
		count++;
	}
}

struct config_t *parse(FILE *f)
{
	char *buf;
	struct config_t *ffsb_config;
	struct container_t *tmp_container;
	
	ffsb_config = malloc(sizeof(struct config_t));
	ffsb_config->global = malloc(sizeof(global_options));
	memcpy(ffsb_config->global, global_options, sizeof(global_options));
	ffsb_config->fs_container = NULL;
	ffsb_config->tg_container = NULL;
	
	buf = get_next_line(f);
	
	while (buf) {
		set_option(buf, ffsb_config->global);
		tmp_container = search_group(buf, f);
		if(tmp_container)
			switch (tmp_container->type) {
			case FILESYSTEM:
				if (ffsb_config->fs_container == NULL)
					ffsb_config->fs_container = tmp_container;
				else
					insert_container(ffsb_config->fs_container,
							 tmp_container);
				break;
			case THREAD_GROUP:
				if (ffsb_config->tg_container == NULL)
					ffsb_config->tg_container = tmp_container;
				else
					insert_container(ffsb_config->tg_container,
							 tmp_container);
				break;
			default:
				break;
			}
		buf = get_next_line(f);
	}
	return ffsb_config;
}

#if 0
static char *parse_stats_config(ffsb_statsc_t *fsc, FILE *f, char *buf,
				int *need_stats)
{
	int flag = 1;
	uint32_t temp32 = 0;
	double tempdouble1;
	double tempdouble2;

	ffsb_statsc_init(fsc);

	while (flag) {
		buf = get_next_line(f);
		if ((buf == NULL) || (0 == ffsb_strnlen(buf, BUFSIZE))) {
			printf("parse error, nothing left to parse "
			       "during stats parsing\n");
			exit(1);
		}

		if (1 == sscanf(buf, "enable_stats=%u", &temp32)) {
			printf("got enable_stats = %u\n", temp32);
			*need_stats = temp32;
			continue;
		}

		if (0 == strncmp(buf, "[end]", strlen("[end]"))) {
			flag = 0;
			buf = get_next_line(f);
			continue;
		}

		if (2 == sscanf(buf, "bucket %lf %lf", &tempdouble1,
				&tempdouble2)) {
			uint32_t min = (uint32_t)(tempdouble1 * 1000.0f);
			uint32_t max = (uint32_t)(tempdouble2 * 1000.0f);
			/* printf("parser: bucket %lf -> %u %lf -> %u\n",
			   tempdouble1,min,tempdouble2,max);*/
			ffsb_statsc_addbucket(fsc, min, max);
			continue;
		}

		if (0 == strncmp(buf, "ignore=", strlen("ignore="))) {
			char *tmp = buf + strlen("ignore=") ;
			unsigned len = ffsb_strnlen(buf, BUFSIZE);
			syscall_t sys;

			if (len == strlen("ignore=")) {
				printf("bad ignore= line\n");
				continue;
			}
			if (ffsb_stats_str2syscall(tmp, &sys)) {
				/* printf("ignoring %d\n",sys); */
				ffsb_statsc_ignore_sys(fsc, sys);
				goto out;
			}

			printf("warning: can't ignore unknown syscall %s\n",
			       tmp);
out:
			continue;
		}

	}
	/* printf("fsc->ignore_stats = 0x%x\n",fsc->ignore_stats); */
	return buf;
}
#endif

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

static unsigned get_num_threadgroups(struct config_t *ffsb_config)
{
	int numtg = 0;
	struct container_t *tgroups = ffsb_config->tg_container;

	while(tgroups) {
		numtg++;
		tgroups = tgroups->next;
	}
	return numtg;
}

static unsigned get_num_filesystems(struct config_t *ffsb_config)
{
	int numfs = 0;
	struct container_t *fs = ffsb_config->fs_container;

	while(fs) {
		numfs++;
		fs = fs->next;
	}
	return numfs;
}

static int get_num_totalthreads(struct config_t *ffsb_config)
{
	int num_threads = 0;
	struct container_t *tg = ffsb_config->tg_container;
	struct config_options_t *tg_config;

	while(tg) {
		tg_config = tg->config;
		while(tg_config->name) {
			if (!strcmp(tg_config->name, "num_threads")) {
				num_threads += *(uint32_t *) tg_config->value;
				}
			tg_config++;
		}
		if (tg->next)
			tg = tg->next;
		else
			break;
	}

	return num_threads;
}

char * get_config_str(struct config_options_t *config, char *name)
{
	struct config_options_t *conf= config;
	while(conf->name) {
		if (!strcmp(conf->name, name))
			return conf->value;
		conf++;
	}
	return NULL;
}

uint32_t get_config_u32(struct config_options_t *config, char *name)
{
	while(config->name) {
		if (!strcmp(config->name, name)) {
			if (config->value)
				return *(uint32_t *)config->value;
			else
				return 0;
		}
		config++;
	}
	return 0;
}

uint8_t get_config_bool(struct config_options_t *config, char *name)
{
	while(config->name) {
		if (!strcmp(config->name, name)) {
			if (config->value)
				return *(uint8_t *)config->value;
			else
				return 0;
		}
		config++;
	}
	return 0;
}

uint32_t get_config_u64(struct config_options_t *config, char *name)
{
	while(config->name) {
		if (!strcmp(config->name, name)) {
			if (config->value)
				return *(uint64_t *)config->value;
			else
				return 0;
		}
		config++;
	}
	return 0;
}

struct config_options_t * get_fs_config(ffsb_config_t *fc, int pos)
{
	struct config_options_t *tmp_config;
	struct container_t *tmp_cont;
	int count = 0;

	assert(pos < fc->num_filesys);

	tmp_cont = fc->config->fs_container;
	while(tmp_cont) {
		tmp_config = tmp_cont->config;
		if (count == pos)
			return tmp_config;
		tmp_cont = tmp_cont->next;
		count++;
	}
	return NULL;
}

struct config_options_t * get_tg_config(ffsb_config_t *fc, int pos)
{
	struct config_options_t *tmp_config;
	struct container_t *tmp_cont;
	int count = 0;

	assert(pos < fc->num_filesys);

	tmp_cont = fc->config->tg_container;
	while(tmp_cont) {
		tmp_config = tmp_cont->config;
		if (count == pos)
			return tmp_config;
		tmp_cont = tmp_cont->next;
		count++;
	}
	return NULL;
}

static void init_filesys(ffsb_config_t *fc, struct config_t *ffsb_config)
{
	struct config_options_t * config;
	ffsb_fs_t *fs;
	int i;

	for (i = 0; i < fc->num_filesys; i++) {
		fs = &fc->filesystems[i];
		memset(fs, 0, sizeof(ffsb_fs_t));
		config = get_fs_config(fc, i);

		fs->basedir = get_config_str(config, "location");
		fs->num_dirs = get_config_u32(config, "num_dirs");
		fs->num_start_files = get_config_u32(config, "num_files");
		fs->minfilesize = get_config_u64(config, "min_filesize");
		fs->maxfilesize = get_config_u64(config, "max_filesize");
		
		fs->flags = 0;
		if (get_config_bool(config, "reuse"))
			fs->flags |= FFSB_FS_REUSE_FS;

		if (get_config_bool(config, "directio"))
			fs->flags |= FFSB_FS_DIRECTIO | FFSB_FS_ALIGNIO4K;

		if (get_config_bool(config, "bufferio"))
			fs->flags |= FFSB_FS_LIBCIO;

		if (get_config_bool(config, "alignio"))
			fs->flags |= FFSB_FS_ALIGNIO4K;

		if (get_config_u32(config, "create_blocksize"))
			fs->create_blocksize = get_config_u32(config,
							      "create_blocksize");
		else
			fs->create_blocksize = FFSB_FS_DEFAULT_CREATE_BLOCKSIZE;

		if (get_config_u32(config, "age_blocksize"))
			fs->age_blocksize = get_config_u32(config, "age_blocksize");
		else
			fs->age_blocksize = FFSB_FS_DEFAULT_AGE_BLOCKSIZE;
		
		fs->age_fs = 0;
	}
}

static void init_groups(ffsb_config_t *fc, struct config_t *ffsb_config)
{
	struct config_options_t * config;
	ffsb_tg_t *tg;
	int i;
	int num_threads;

	for (i=0; i < fc->num_threadgroups; i++) {
		tg = &fc->groups[i];
		memset(tg, 0, sizeof(ffsb_tg_t));
		config = get_tg_config(fc, i);

		num_threads = get_config_u32(config, "num_threads");

		init_ffsb_tg(tg, num_threads, i);

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
			printf("threadgroup %d verification failed\n", i);
			exit(1);
		}

	}
}

static void init_config(ffsb_config_t *fc, struct config_t *ffsb_config)
{
	fc->time = get_config_u32(ffsb_config->global, "time");
	fc->num_filesys = get_num_filesystems(ffsb_config);
	fc->num_threadgroups = get_num_threadgroups(ffsb_config);
	fc->num_totalthreads = get_num_totalthreads(ffsb_config);
	fc->config = ffsb_config;
	fc->callout = get_config_str(ffsb_config->global, "callout");

	fc->filesystems = ffsb_malloc(sizeof(ffsb_fs_t) * fc->num_filesys);
	init_filesys(fc, ffsb_config);

	fc->groups = ffsb_malloc(sizeof(ffsb_tg_t) * fc->num_threadgroups);
	init_groups(fc, ffsb_config);
}

void ffsb_parse_newconfig(ffsb_config_t *fc, char *filename)
{
	FILE *f;

	struct config_t *ffsb_config;

	f = fopen(filename, "r");
	if (f == NULL) {
		perror(filename);
		exit(1);
	}
	ffsb_config = parse(f);
	fclose(f);

	init_config(fc, ffsb_config);

	print_config(ffsb_config);
}
