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
#ifndef _FH_H_
#define _FH_H_


#include <inttypes.h>

struct ffsb_thread;
struct ffsb_fs;

int fhopenread(char *filename, struct ffsb_thread *, struct ffsb_fs *);
int fhopenwrite(char *filenam, struct ffsb_thread *, struct ffsb_fs *);
int fhopencreate(char *filename, struct ffsb_thread *, struct ffsb_fs *);
int fhopenappend(char *filename, struct ffsb_thread *, struct ffsb_fs *);

void fhread(int fd,void *buf,uint64_t size, 
	    struct ffsb_thread *, struct ffsb_fs * );

/* can only write up to size_t bytes at a time, so size is a uint32_t */
void fhwrite(int fd,void *buf,uint32_t size, 
	     struct ffsb_thread *, struct ffsb_fs * );
void fhseek(int fd,uint64_t offset,int whence, 
	    struct ffsb_thread *, struct ffsb_fs * );
void fhclose(int fd, 
	     struct ffsb_thread *, struct ffsb_fs * );

int writefile_helper(int fd, uint64_t size, uint32_t blocksize, char* buf,
		     struct ffsb_thread *ft, struct ffsb_fs *fs);



#endif
