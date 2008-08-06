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
		ret = malloc(sizeof(uint64_t));
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
	/*printf("\n");
	  while (ret_container->config->name){
	  printf ("%s\n", ret_container->config->name);
	  ret_container->config++;
	  }
	  printf("\n");*/
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
			printf("%s", config->value);
			break;
		case TYPE_BOOLEAN:
			break;
	}
}

static void print_config(struct config_t *config)
{
	uint32_t tmp;
	int count = 0;

	struct config_options_t *tmp_config;
	struct container_t *tmp_cont;

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

static char *parse_globals(ffsb_config_t *fc, FILE *f, unsigned *fs_flags)
{
	
	//parse(f);
	exit(1);
	
	return NULL;
}

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

static char *parse_all_fs(ffsb_config_t *fc, FILE *f, char *buf,
			  unsigned fs_flags)
{
	unsigned numfs = fc_get_num_filesys(fc);
	int i;
	//uint32_t temp32;
	ffsb_fs_t zero_default;
	//ffsb_fs_t *dft = &zero_default;
	char search_str[256];
	
	memset(&zero_default, 0, sizeof(zero_default));

	for (i = 0; i < numfs; i++) {
		if ((buf == NULL) || (0 == ffsb_strnlen(buf, BUFSIZE))) {
			printf("parse error before filesystem %u\n", i);
			printf("filesystem clause appears to be missing\n");
			exit(1);
		}
		sprintf(search_str,	"[filesystem%u]", i);
		/*if (search_group(f, search_str) != -1) {
			buf = get_next_line(f);
			buf = parse_fs(fc_get_fs(fc, i), i, f, buf,
				       fs_flags, dft);
			dft = fc_get_fs(fc, i);
		} else {
			fprintf(stderr, "wtf ??: %s\n", buf);
		}*/
	}
	return buf;
}

void ffsb_parse_newconfig(ffsb_config_t *fc, char *filename)
{
	FILE *f;

	int i, numtg, num_threads = 0;
	struct config_t *ffsb_config;

	f = fopen(filename, "r");
	if (f == NULL) {
		perror(filename);
		exit(1);
	}
	ffsb_config = parse(f);
	print_config(ffsb_config);
	
	numtg = fc_get_num_threadgroups(ffsb_config);
	printf("Threadgroup count = %d\n", numtg);
	num_threads = fc_get_num_totalthreads(ffsb_config);
	printf("Total Threads = %d\n", num_threads);

	fc_set_num_totalthreads(fc, num_threads);

	fclose(f);
}
