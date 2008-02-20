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

void parseerror(char *msg) {
	fprintf(stderr, "Error parsing %s\n", msg);
	exit(1);
}

/* strips out whitespace and comments, returns NULL on eof */
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

static char *parse_globals(ffsb_config_t *fc, FILE *f, unsigned *fs_flags)
{
	char *buf = get_next_line(f);
	uint32_t numfs = 0, numtg = 0, dio = 0, verbose = 0, time = 0, bio = 0,
	    aio = 0, callout_flag = 0;
	uint32_t temp32;
	char callout_buf[4096];

	if (1 == sscanf(buf, "num_filesystems=%u\n", &temp32)) {
		numfs = temp32;
		buf = get_next_line(f);
	}

	if (1 == sscanf(buf, "num_threadgroups=%u\n", &temp32)) {
		numtg = temp32;
		buf = get_next_line(f);
	}

	if (1 == sscanf(buf, "verbose=%u\n", &temp32)) {
		verbose = temp32;
		buf = get_next_line(f);
	}

	if (1 == sscanf(buf, "directio=%u\n", &temp32)) {
		dio = temp32;
		buf = get_next_line(f);
	}
	if (1 == sscanf(buf, "bufferedio=%u\n", &temp32)) {
		bio = temp32;
		buf = get_next_line(f);
	}

	if (1 == sscanf(buf, "alignio=%u\n", &temp32)) {
		aio = temp32;
		buf = get_next_line(f);
	}

	if (1 == sscanf(buf, "time=%u\n", &temp32)) {
		time = temp32;
		buf = get_next_line(f);
	}

	if (1 == sscanf(buf, "callout=%4087s\n", callout_buf)) {
		callout_flag = 1;
		strncpy(callout_buf, buf + strlen("callout="), 4096);
		buf = get_next_line(f);
	}

	if (numfs < 1 || numtg < 1) {
		fprintf(stderr, "possible parse error or invalid settings\n");
		fprintf(stderr, "ffsb config must have at least 1 filesystem "
			"and 1 threadconfig, aborting \n");
		fclose(f);
		exit(1);
	}
	if (time < 0) {
		fprintf(stderr, "possible parse error or invalid settings\n");
		fprintf(stderr, "ffsb must run for at least 1 second :), "
			"aborting time = %u\n", time);
		fclose(f);
		exit(1);
	}

	if (dio)
		*fs_flags |= FFSB_FS_DIRECTIO | FFSB_FS_ALIGNIO4K;
	if (bio)
		*fs_flags |= FFSB_FS_LIBCIO;
	if (aio)
		*fs_flags |= FFSB_FS_ALIGNIO4K;

	init_ffsb_config(fc, numfs, numtg);
	fc_set_time(fc, time);
	if (callout_flag) {
		printf("parser: callout found \"%s\"\n", callout_buf);
		fc_set_callout(fc, callout_buf);
	}

	return buf;
}

static char *parse_stats_config(ffsb_statsc_t *fsc, FILE *f, char *buf,
				int *need_stats)
{
	int flag = 1;
	uint32_t temp32 = 0;
	double tempdouble1;
	double tempdouble2;
	/* unsigned num_buckets = 0; */
	/* unsigned cur_bucket = 0; */

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

static char *parse_tg(ffsb_tg_t *tg, int tgnum, FILE *f, char *buf,
		      ffsb_tg_t *dft)
{
	int flag = 1;
	int bindfs = -1;
	uint32_t temp32 = 0;
	uint64_t temp64 = 0;
	uint32_t numthreads = tg_get_numthreads(dft);

	uint32_t rwgt = 0;   /* read weight */
	uint32_t rawgt = 0;  /* readall weight */
	uint32_t wwgt = 0;   /* write weight */
	uint32_t cwgt = 0;   /* create weight */
	uint32_t awgt = 0;   /* append weight  */
	uint32_t dwgt = 0;   /* delete weight */
	uint32_t mwgt = 0;   /* meta weight */
	uint32_t cdwgt = 0;  /* createdir weight */

	uint64_t rsize  = tg_get_read_size(dft);
	uint32_t rbsize = tg_get_read_blocksize(dft);
	uint64_t wsize  = tg_get_write_size(dft);
	uint32_t wbsize = tg_get_write_blocksize(dft);
	uint32_t rr     = tg_get_read_random(dft);
	uint32_t wr     = tg_get_write_random(dft);
	uint32_t fsync  = tg_get_fsync_file(dft);

	uint32_t rskip  = tg_get_read_skip(dft);
	uint32_t rskipsize = tg_get_read_skipsize(dft);

	unsigned waittime = tg_get_waittime(dft);

	ffsb_statsc_t fsc = { 0, };
	int need_stats = 0;

	while (flag) {
		if ((buf == NULL) || (0 == ffsb_strnlen(buf, BUFSIZE))) {
			printf("parse error, nothing left to parse "
			       "for threadgroup %u\n", tgnum);
			exit(1);
		}

		if (1 == sscanf(buf, "bindfs=%d\n", &temp32)) {
			bindfs = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "num_threads=%u\n", &temp32)) {
			numthreads = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "read_weight=%u\n", &temp32)) {
			rwgt = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "readall_weight=%u\n", &temp32)) {
			rawgt = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "read_random=%u\n", &temp32)) {
			rr = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "read_skip=%u\n", &temp32)) {
			rskip = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "read_size=%llu\n", &temp64)) {
			rsize = temp64;
			buf = get_next_line(f);
			continue;
		}

		if (1 == sscanf(buf, "read_blocksize=%u\n", &temp32)) {
			rbsize = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "read_skipsize=%u\n", &temp32)) {
			rskipsize = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "write_weight=%u\n", &temp32)) {
			wwgt = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "write_random=%u\n", &temp32)) {
			wr = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "fsync_file=%u\n", &temp32)) {
			fsync = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "write_size=%llu\n", &temp64)) {
			wsize = temp64;
			buf = get_next_line(f);
			continue;
		}

		if (1 == sscanf(buf, "write_blocksize=%u\n", &temp32)) {
			wbsize = temp32;
			buf = get_next_line(f);
			continue;
		}

		if (1 == sscanf(buf, "create_weight=%u\n", &temp32)) {
			cwgt = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "delete_weight=%u\n", &temp32)) {
			dwgt = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "append_weight=%u\n", &temp32)) {
			awgt = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "meta_weight=%u\n", &temp32)) {
			mwgt = temp32;
			buf = get_next_line(f);
			continue;
		}

		if (1 == sscanf(buf, "createdir_weight=%u\n", &temp32)) {
			cdwgt = temp32;
			buf = get_next_line(f);
			continue;
		}

		if (1 == sscanf(buf, "op_delay=%u\n", &temp32)) {
			waittime = temp32;
			buf = get_next_line(f);
			continue;
		}

		if (0 == strncmp(buf, "[stats]", strlen("[stats]"))) {
			buf = parse_stats_config(&fsc, f, buf, &need_stats);
			continue;
		}

		if (1 == sscanf(buf, "[end%u]\n", &temp32)) {
			if (temp32 != tgnum)
				fprintf(stderr, "parse_tg: tgnum isn't %u!!!\n",
					tgnum);
			flag = 0;
			buf = get_next_line(f);
			continue;
		}
		fprintf(stderr, "parse error, unknown line: %s\n", buf);
		buf = get_next_line(f);
	}
	if (numthreads < 1) {
		fprintf(stderr, "thread group configuration is invalid "
			"(0 threads), aborting\n");
		fclose(f);
		exit(1);
	}

	init_ffsb_tg(tg, numthreads, tgnum);

	if (need_stats)
		tg_set_statsc(tg, &fsc);

	tg_set_bindfs(tg, bindfs);

	tg_set_read_random(tg, rr);
	tg_set_write_random(tg, wr);
	tg_set_fsync_file(tg, fsync);

	tg_set_read_size(tg, rsize);
	tg_set_read_blocksize(tg, rbsize);

	tg_set_read_skip(tg, rskip);
	tg_set_read_skipsize(tg, rskipsize);

	tg_set_write_size(tg, wsize);
	tg_set_write_blocksize(tg, wbsize);

	tg_set_op_weight(tg, "read", rwgt);
	tg_set_op_weight(tg, "readall", rawgt);
	tg_set_op_weight(tg, "write", wwgt);
	tg_set_op_weight(tg, "append", awgt);
	tg_set_op_weight(tg, "create", cwgt);
	tg_set_op_weight(tg, "delete", dwgt);
	tg_set_op_weight(tg, "metaop", mwgt);
	tg_set_op_weight(tg, "createdir", cdwgt);

	tg_set_waittime(tg, waittime);

	if (verify_tg(tg)) {
		printf("threadgroup %d verification failed\n", tgnum);
		exit(1);
	}

	return buf;
}

static char *parse_fs(ffsb_fs_t *fs, int fsnum, FILE *f, char *buf,
		      unsigned fs_flags, ffsb_fs_t *def_fs)
{
	char tempbuf[256];

	int flag = 1;
	uint64_t temp64;
	uint32_t temp32;

	int reuse            = fs_get_reuse_fs(def_fs);
	int agefs            = fs_get_agefs(def_fs);
	uint32_t numfiles    = fs_get_numstartfiles(def_fs);
	uint32_t numdirs     = fs_get_numdirs(def_fs);
	uint64_t minfilesize = fs_get_min_filesize(def_fs);
	uint64_t maxfilesize = fs_get_max_filesize(def_fs);

	uint32_t create_blksize = fs_get_create_blocksize(def_fs);
	uint32_t age_blksize    = fs_get_age_blocksize(def_fs);

	double tempdouble;
	double fsutil = fs_get_desired_fsutil(def_fs);
	ffsb_tg_t *atg = fs_get_aging_tg(def_fs);

	ffsb_statsc_t fsc = { 0, };
	int need_stats = 0;

	memset(tempbuf, 0, 256);

	while (flag) {
		if ((buf == NULL) || (0 == ffsb_strnlen(buf, BUFSIZE))) {
			printf("parse error, nothing left to parse "
			       "for filesystem %u\n", fsnum);
			exit(1);
		}
		if (1 == sscanf(buf, "location=%256s\n", tempbuf)) {
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "num_files=%u\n", &temp32)) {
			numfiles = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "num_dirs=%u\n", &temp32)) {
			numdirs = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "reuse=%u\n", &temp32)) {
			reuse = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "max_filesize=%llu\n", &temp64)) {
			maxfilesize = temp64;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "min_filesize=%llu\n", &temp64)) {
			minfilesize = temp64;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "create_blocksize=%u\n", &temp32)) {
			create_blksize = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "age_blocksize=%u\n", &temp32)) {
			age_blksize = temp32;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "desired_util=%lf\n", &tempdouble)) {
			fsutil = tempdouble;
			buf = get_next_line(f);
			continue;
		}
		if (1 == sscanf(buf, "agefs=%u\n", &temp32)) {
			agefs = temp32;
			buf = get_next_line(f);
			if (agefs != 0) {
				ffsb_tg_t dft;
				memset(&dft, 0, sizeof(dft));
				if (1 != sscanf(buf, "[threadgroup%u]\n",
						&temp32)) {
					fprintf(stderr, "Error, [threadgroup0] "
						"must follow agefs parameter: "
						"buf was %s\n", buf);
					agefs = 0;
					continue;
				}
				atg = ffsb_malloc(sizeof(ffsb_tg_t));
				buf = get_next_line(f);
				buf = parse_tg(atg, 0, f, buf, &dft);
			}
			continue;
		}

		if (0 == strncmp(buf, "[stats]", strlen("[stats]"))) {
			buf = parse_stats_config(&fsc, f, buf, &need_stats);
			continue;
		}

		if (1 == sscanf(buf, "[end%u]\n", &temp32)) {
			if (temp32 != fsnum)
				fprintf(stderr,
					"parse_fs: %d fsnum isn't %u!\n",
					temp32, fsnum);
			flag = 0;
			buf = get_next_line(f);
			continue;
		}
		fprintf(stderr, "parse error, unexpected line: %s\n", buf);
		buf = get_next_line(f);
	}

	if (strlen(tempbuf) == 0) {
		fprintf(stderr, "filesystems must have a location, aborting\n");
		fclose(f);
		exit(1);
	}

	init_ffsb_fs(fs, tempbuf, numdirs, numfiles, fs_flags);

	fs_set_min_filesize(fs, minfilesize);
	fs_set_max_filesize(fs, maxfilesize);
	fs_set_reuse_fs(fs, reuse);

	if (create_blksize)
		fs_set_create_blocksize(fs, create_blksize);
	if (age_blksize)
		fs_set_age_blocksize(fs, age_blksize);
	if (agefs)
		fs_set_aging_tg(fs, atg, fsutil);

	if (need_stats)
		fprintf(stderr, "warning, per filsystem statistics not "
			"implemented yet\n");

	return buf;
}

static char *parse_all_fs(ffsb_config_t *fc, FILE *f, char *buf,
			  unsigned fs_flags)
{
	unsigned numfs = fc_get_num_filesys(fc);
	int i;
	uint32_t temp32;
	ffsb_fs_t zero_default;
	ffsb_fs_t *dft = &zero_default;

	memset(&zero_default, 0, sizeof(zero_default));

	for (i = 0; i < numfs; i++) {
		if ((buf == NULL) || (0 == ffsb_strnlen(buf, BUFSIZE))) {
			printf("parse error before filesystem %u\n", i);
			printf("filesystem clause appears to be missing\n");
			exit(1);
		}
		if (1 == sscanf(buf, "[filesystem%u] ", &temp32)) {
			buf = get_next_line(f);
			buf = parse_fs(fc_get_fs(fc, i), temp32, f, buf,
				       fs_flags, dft);
			dft = fc_get_fs(fc, i);
		} else {
			fprintf(stderr, "wtf ??: %s\n", buf);
		}
	}
	return buf;
}

static char *parse_all_tgs(ffsb_config_t *fc, FILE *f, char *buf)
{
	unsigned numtg = fc_get_num_threadgroups(fc);
	int i;
	uint32_t temp32;
	ffsb_tg_t zero_default;
	ffsb_tg_t *dft = &zero_default;

	memset(&zero_default, 0, sizeof(zero_default));

	for (i = 0; i < numtg; i++) {
		if ((buf == NULL) || (0 == ffsb_strnlen(buf, BUFSIZE))) {
			printf("parse error before threadgroup %u\n", i);
			printf("threadgroup clause appears to be missing\n");
			exit(1);
		}
		if (1 == sscanf(buf, "[threadgroup%u] ", &temp32)) {
			buf = get_next_line(f);
			buf = parse_tg(fc_get_tg(fc, i), temp32, f, buf, dft);
			dft = fc_get_tg(fc, i);
		} else {
			fprintf(stderr, "parser: expecting [threadgroup%u] "
				"got : %s\n", i, buf);
		}
	}
	return buf;
}

void ffsb_parse_newconfig(ffsb_config_t *fc, char *filename)
{
	FILE *f;
	char *buf;
	int i, numtg, num_threads = 0;
	unsigned fs_flags = 0;

	f = fopen(filename, "r");
	if (f == NULL) {
		perror(filename);
		exit(1);
	}
	buf  = parse_globals(fc, f, &fs_flags);
	buf  = parse_all_fs(fc, f, buf, fs_flags);
	buf  = parse_all_tgs(fc, f, buf);

	if (buf != NULL)
		fprintf(stderr, "extra stuff at end of config file: %s\n", buf);

	numtg = fc_get_num_threadgroups(fc);
	for (i = 0; i < numtg; i++)
		num_threads += tg_get_numthreads(fc_get_tg(fc, i));

	fc_set_num_totalthreads(fc, num_threads);

	fclose(f);
}
