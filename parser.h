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
#ifndef _PARSER_H_
#define _PARSER_H_

#include "ffsb.h"

#define COMMENT_CHAR	'#'

#define TYPE_U32		0x0001
#define	TYPE_U64		0x0002
#define TYPE_STRING		0x0004
#define TYPE_BOOLEAN	0x0008

#define ROOT			0x0001
#define THREAD_GROUP	0x0002
#define FILESYSTEM		0x0004
#define END				0x0008
#define STATS			0x0010

struct container_t {
	struct config_options_t *config;
	uint32_t type;
	struct container_t *child;
	struct container_t *next;
};

struct config_options_t {
	char *name;
	void *value;
	int type;
};

struct container_desc_t {
	char *name;
	uint16_t type;
	uint16_t size;
};

void ffsb_parse_newconfig(ffsb_config_t *fc, char *filename);

#endif
