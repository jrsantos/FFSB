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
/* treap.h Version 2.0 DH, 03/04 */
/* ***************************** */

#ifndef _Treap_H
#define _Treap_H


#include <stdlib.h>
#include <limits.h>
#include "filelist.h"

typedef struct ffsb_file * datatype ;

#define GETKEY( a ) ( (a)->num )


#define infinity INT_MAX  /* Has to be set to 'infinity' */

struct treap_node
 {
  datatype    object;
  struct treap_node *       left;
  struct treap_node *       right;
  int         prio;
 };



typedef struct treap_node * position;
typedef struct treap_node * treap;

treap treap_empty(treap T);
position find_node(datatype X, treap T);
position find_min(treap T);
position find_max(treap T);
treap initialize(void);
treap insert(datatype X, treap T);
treap rm_node(datatype X, treap T);
treap rec_rm(datatype X, treap T);
treap root_rm(treap T);
datatype retrieve(position P);

extern position nullnode;

#endif  /* _Treap_H */
