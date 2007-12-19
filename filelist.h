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
#ifndef _FILELIST_H_
#define _FILELIST_H_
#include <pthread.h>
#include "rand.h"
#include "rwlock.h"
#include "cirlist.h"
#include "rbt.h"

#define SUBDIRNAME_BASE "dir"
#define FILENAME_BASE   "file"

struct ffsb_file {
	char *name;
	uint64_t size;
	struct rwlock lock;
	uint32_t num;
};

struct cirlist;

/* Tree of ffsb_file structs and associated state info */
/* struct must be locked during use */
struct benchfiles {
	/* the base directory in which all subdirs and files are created */
	char *basedir;
	
	/* the name to prepend to all directory and file names */
	char *basename;
	uint32_t numsubdirs;
	
	/* files which currently exist on the filesystem */
	struct red_black_tree* files;
	
	/* files which have been deleted, and whose numbers should be reused */
	struct cirlist* holes;

	/* this lock must be held while manipulating the structure */
	struct rwlock fileslock;
	uint32_t listsize; /* sum size of nodes in files and holes */
};


/* Initializes the list, user must call this before anything else */
/* it will create the basedir and subdirs on the filesystem automatically */
/* if the builddirs arg. is nonzero */
void init_filelist(struct benchfiles *bf, char * basedir, char * basename, uint32_t numsubdirs, int builddirs);

void destroy_filelist(struct benchfiles *bf);

/* allocates a new file, adds to list, (write) locks it, and returns it */
/* This function also randomly selects a filename + path to assign to */
/* the new file */
/* it first checks the "holes" list for any available filenames */
/* caller must ensure file is actually created on disk */
struct ffsb_file * add_file(struct benchfiles *b, uint64_t size,randdata_t *rd);

/* removes file from list, decrements listsize */
/* file should be writer-locked before calling this function */
/* this function does not unlock file after removal from list */
/* caller must ensure file is actually removed on disk */
/* caller must NOT free file->name and file, since oldfiles are being put into holes list */
void remove_file(struct benchfiles *b, struct ffsb_file *file);

/* picks a file at random, locks it for reading and returns it locked */
struct ffsb_file * choose_file_reader(struct benchfiles *b, randdata_t *);

/* picks a file at random, locks it for writing and returns it locked */
struct ffsb_file * choose_file_writer(struct benchfiles *b, randdata_t *);

/* changes the file->name of a file, file must be write locked  */
/* it does not free the old file->name, so caller must keep a ref to it */
/* and free after the call */
void rename_file( struct ffsb_file * file);

void unlock_file_reader(struct ffsb_file * file);
void unlock_file_writer(struct ffsb_file * file);

/* uses SUBDIRNAME_BASE/FILENAME_BASE + bf->basename to validate a name */
/* returns a negative on invalid names, and the actual file number if valid */
int validate_filename( struct benchfiles *bf, char *name);
int validate_dirname ( struct benchfiles *bf, char *name);

/* function type which, does some validation of existing files */
/* currently only used by ffsb_fs stuff, returns 0 on success */
typedef int (*fl_validation_func_t)(struct benchfiles*, char *, void* );

/* provided for re-use of filesets, and runs the validation callback on each */
/* file/dir that is found, after verifying the name is conformant */
/* the fileset should be initialized with init_fileset() beforehand */
/* returns 0 on success */
int grab_old_fileset( struct benchfiles *bf, char *basename, 
		      fl_validation_func_t  vfunc, void * vfunc_data);

/* get the number of files */
uint32_t get_listsize(struct benchfiles *bf);

/* get the number of subdirectories */
uint32_t get_numsubdirs(struct benchfiles *bf);

#endif
