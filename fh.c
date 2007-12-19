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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <assert.h>

#include "ffsb.h"
#include "fh.h"

#include "config.h"

/* !!! ugly */
#ifndef HAVE_OPEN64 
 #define open64 open
#endif

#ifndef HAVE_FSEEKO64
 #define lseek64 lseek
#endif

/*
  all these functions read the global mainconfig->bufferedio
  variable to determine if they are to do buffered i/o or normal
*/

int fhopenhelper(char *filename,char* bufflags, int flags){
	int fd = 0;
	if ((fd=open64(filename,flags, S_IRWXU)) < 0 ){
		perror(filename);
		exit(0);
	}
	return fd;
}

int fhopenread(char *filename, int directio ){
	if( ! directio  )
		return fhopenhelper(filename,"r",O_RDONLY);
	else
		return fhopenhelper(filename,"r",O_RDONLY|O_DIRECT);
}

int fhopenappend(char *filename, int directio){
	if(! directio )
		return fhopenhelper(filename,"a",O_APPEND|O_WRONLY);
	else
		return fhopenhelper(filename,"a",O_DIRECT|O_APPEND|O_WRONLY);
}

int fhopenwrite(char *filename, int directio){
	if(! directio )
		return fhopenhelper(filename,"w",O_WRONLY);
	else
		return fhopenhelper(filename,"w",O_DIRECT|O_WRONLY);
}


int fhopencreate(char *filename, int directio){
	return fhopenhelper(filename,"rw",O_CREAT | O_RDWR | O_TRUNC);
}

void fhread(int fd,void *buf,uint64_t size ){
	ssize_t realsize;

	assert ( size <= SIZE_MAX);
	realsize=read(fd ,buf, size);

	if (realsize!=size){
		printf("Read %d instead of %lld bytes.\n",realsize,size);
		/* printf("buf of size %lld started at %p \n",size,buf); */
		perror("read");
		exit(1);
	}
}

void fhwrite(int fd,void *buf,uint32_t size ){
	ssize_t realsize;

	assert(size <= SIZE_MAX );
	realsize=write(fd ,buf,size);

	if (realsize!=size){
		
		printf("Wrote %d instead of %d bytes.\n"
			  "Probably out of disk space\n",realsize,size);
/* 		printf("buf of size %d started at %p \n",size,buf); */
		perror("write");
		exit(1);
	}
}

void fhseek(int fd,uint64_t offset,int whence ){
	uint64_t res;

	if((whence==SEEK_CUR)&&(offset==0))
		return;
	res = lseek64(fd,offset,whence);
	if( (whence == SEEK_SET) && (res != offset)) {
	        perror("seek");
	}
	if (res==-1){
		if (whence==SEEK_SET){
			fprintf(stderr,"tried to seek to %lld\n",offset);
		}
		else{
			fprintf(stderr,"tried to seek from current position to %lld\n",offset);
		}
		perror("seek");
		exit(1);
	}
}

void fhclose(int fd){
	close(fd);
}
