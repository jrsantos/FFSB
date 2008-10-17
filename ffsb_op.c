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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ffsb_op.h"
#include "fileops.h"
#include "metaops.h"

ffsb_op_t ffsb_op_list[] =
{{0, "read", ffsb_readfile, NULL, ffsb_read_print_exl, fop_bench, NULL},
 {1, "readall",	ffsb_readall, NULL, ffsb_read_print_exl, fop_bench, NULL},
 {2, "write", ffsb_writefile, NULL, ffsb_write_print_exl, fop_bench, NULL},
 {3, "create", ffsb_createfile, NULL, ffsb_create_print_exl, fop_bench, fop_age},
 {4, "append", ffsb_appendfile, NULL, ffsb_append_print_exl, fop_bench, fop_age},
 {5, "delete", ffsb_deletefile, NULL, NULL, fop_bench, fop_age},
 {6, "metaop", ffsb_metaops, NULL, NULL, metaops_metadir, NULL},
 {7, "createdir", ffsb_createdir, NULL, NULL, fop_bench, NULL},
 {8, "stat", ffsb_stat, NULL, NULL, fop_bench, NULL},
 {9, "writeall", ffsb_writeall, NULL, ffsb_write_print_exl, fop_bench, NULL},
 {10, "writeall_fsync", ffsb_writeall_fsync, NULL, ffsb_write_print_exl, fop_bench, NULL},
 {11, "open_close", ffsb_open_close, NULL, NULL, fop_bench, NULL},
};

void init_ffsb_op_results(ffsb_op_results_t *results)
{
	memset(results, 0, sizeof(ffsb_op_results_t));
}

static int exclusive_op(ffsb_op_results_t *results, unsigned int op_num)
{
	int i;
	int ret = 0;
	for (i = 0; i < FFSB_NUMOPS ; i++) {
		if (i == op_num)
			continue;
		ret += results->ops[i];
	}

	if (ret)
		return 0;
	return 1;
}

static void generic_op_print(char *name, unsigned num, double op_pcnt,
			     double weigth_pcnt, double runtime)
{
	printf("%20s : %12u\t%10.2lf\t%6.3lf%%\t\t%6.3lf%%\n",
	       name, num, num/runtime, op_pcnt, weigth_pcnt);
}

static void print_op_results(unsigned int op_num, ffsb_op_results_t *results,
			     double runtime, unsigned total_ops,
			     unsigned total_weight)
{
	if (exclusive_op(results, op_num) &&
	    ffsb_op_list[op_num].op_exl_print_fn != NULL) {
		ffsb_op_list[op_num].op_exl_print_fn(results, runtime, op_num);
	} else {
	    double op_pcnt = 100 * (double)results->ops[op_num] /
		    (double)total_ops;
	    double weight_pcnt = 100 * (double)results->op_weight[op_num] /
		    (double)total_weight;
	    generic_op_print(ffsb_op_list[op_num].op_name, results->ops[op_num],
			     op_pcnt, weight_pcnt, runtime);
	}
}

void print_results(struct ffsb_op_results *results, double runtime)
{
	int i;
	uint64_t total_ops = 0;
	uint64_t total_weight = 0;

	for (i = 0; i < FFSB_NUMOPS ; i++) {
		total_ops += results->ops[i];
		total_weight += results->op_weight[i];
	}

	printf("             Op Name   Transactions\t Trans/sec\t% Trans\t    % Op Wegiht\n");
	printf("             =======   ============\t =========\t=======\t    ===========\n");
	for (i = 0; i < FFSB_NUMOPS ; i++)
		if (results->ops[i] != 0)
			print_op_results(i, results, runtime, total_ops,
					 total_weight);

	printf("-\n%.2lf Transactions per Second\n", (double)total_ops / runtime);
}


char *op_get_name(int opnum)
{
	return ffsb_op_list[opnum].op_name;
}

void ops_setup_bench(ffsb_fs_t *fs)
{
	int i;
	for (i = 0; i < FFSB_NUMOPS; i++)
		ffsb_op_list[i].op_bench(fs, i);
}

void ops_setup_age(ffsb_fs_t *fs)
{
	int i;
	for (i = 0; i < FFSB_NUMOPS; i++)
		if (ffsb_op_list[i].op_age)
		    ffsb_op_list[i].op_age(fs, i);
}

int ops_find_op(char *opname)
{
	int i;
	for (i = 0; i < FFSB_NUMOPS; i++)
		if (!strcmp(opname, ffsb_op_list[i].op_name))
			return i;
	return -1;
}

void add_results(struct ffsb_op_results *target, struct ffsb_op_results *src)
{
	int i;
	target->read_bytes += src->read_bytes;
	target->write_bytes += src->write_bytes;

	for (i = 0; i < FFSB_NUMOPS; i++) {
		target->ops[i] += src->ops[i];
		target->op_weight[i] += src->op_weight[i];
	}
}

void do_op(struct ffsb_thread *ft, struct ffsb_fs *fs, unsigned op_num)
{
	ffsb_op_list[op_num].op_fn(ft, fs, op_num);
}
