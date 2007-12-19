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
#ifndef _FFSB_THREAD_H_
#define _FFSB_THREAD_H_

#include <pthread.h>
#include <inttypes.h>

#include "rand.h"
#include "ffsb_op.h"
#include "ffsb_tg.h"

#include "util.h" /* for barrier stuff */

struct ffsb_tg;
struct ffsb_op_results;

/* ffsb thread object */
/* The thread object doesn't store any configuration information, it mostly */
/* just holds per-thread state information such as the random data store */
/* and the per-thread buffer to copy data to/from disk */

typedef struct ffsb_thread {
	unsigned thread_num;
	unsigned tg_num;
	pthread_t ptid;
	struct randdata rd;
	struct ffsb_tg * tg; /* owning thread group */

/* If we are using Direct IO, then we must only use a 4k aligned buffer*/
/* so, alignedbuf_4k is a pointer into "mallocbuf" which is what malloc gave us */	
	char *alignedbuf; 	
	char *mallocbuf;

	struct ffsb_op_results results;
} ffsb_thread_t ;

/* */
void        init_ffsb_thread(ffsb_thread_t *ft,struct ffsb_tg *tg, unsigned bufsize, unsigned tg_num, unsigned thread_num); 
void        destroy_ffsb_thread(ffsb_thread_t *ft);

/* owning thread group will start thread with this, thread runs */
/* until *ft->checkval == ft->stopval */
/* Yes this is not strictly synchronized, but that is okay for our */
/* purposes, and it limits (IMO expensive) bus-locking */

/* pthread_create() is called by tg with this function as a parameter */
/* data is a (ffsb_thread_t*) */
void*       ft_run( void * data );

void        ft_alter_bufsize(ffsb_thread_t *ft, unsigned bufsize);
char*       ft_getbuf(ffsb_thread_t *ft);

int         ft_get_read_random(ffsb_thread_t *ft);
uint32_t   ft_get_read_size(ffsb_thread_t *ft);
uint32_t   ft_get_read_blocksize(ffsb_thread_t *ft);

int         ft_get_write_random(ffsb_thread_t *ft);
uint32_t   ft_get_write_size(ffsb_thread_t *ft);
uint32_t   ft_get_write_blocksize(ffsb_thread_t *ft);

int        ft_get_fsync_file(ffsb_thread_t *ft);

randdata_t* ft_get_randdata(ffsb_thread_t *ft);

void        ft_incr_op(ffsb_thread_t *ft, unsigned opnum, unsigned increment);

void        ft_add_readbytes(ffsb_thread_t *ft, uint32_t bytes );
void        ft_add_writebytes(ffsb_thread_t *ft, uint32_t bytes );

int         ft_get_read_skip(ffsb_thread_t *ft);
uint32_t    ft_get_read_skipsize(ffsb_thread_t *ft);

ffsb_op_results_t *ft_get_results(ffsb_thread_t *ft);

#endif
