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
/* ************************************************************** */
/*                                                                */
/* Treap - Randomized Search Tree                                 */
/*                                                                */
/* Pugh (1988), Seidel & Aragon (1989)                            */
/* Weiss (1996), D.Heger (2004)                                   */
/*                                                                */
/* Version 2.0 (03/04) DH                                         */
/*                                                                */
/* ************************************************************** */

#include <stdlib.h>
#include "treap.h"


position nullnode = NULL;  /* Needs initialization */

treap initialize(void)
 {
  if(nullnode == NULL)
   {
    nullnode = malloc(sizeof(struct treap_node));
    if(nullnode == NULL) printf("Out of Memory\n");
    nullnode->left = nullnode->right = nullnode;
    nullnode->prio = infinity;
   }
  return nullnode;
 }

/* rand() Random Number Generator - Should probably be changed .... */

int random_gen(void)
 {
  return rand() - 1;
 }

treap treap_empty(treap T)
 {
  if(T != nullnode)
   {
    treap_empty(T->left);
    treap_empty(T->right);
    free(T);
   }
  return nullnode;
 }

void treap_print(treap T)
 {
  if(T != nullnode)
   {
    treap_print(T->left);
    printf("%d ",T->object);
    treap_print(T->right);
   }
 }

position find_node(datatype X, treap T)
 {
  while(T)
  {
   if(T == nullnode) return NULL;
   if(GETKEY(X) < GETKEY(T->object))
     T = T->left;
   else if(GETKEY(X) > GETKEY(T->object))
     T = T->right;
   else return T;
  }
 }

position find_min(treap T)
 {
  if(T == nullnode) return NULL;
  T = (T->left == nullnode) ? T=T : find_min(T->left);
  return T;
 }

position find_max(treap T)
 {
  if(T != nullnode)
   while(T->right != nullnode)
     T = T->right;
   return T;
 }

/* This function can be called only if node (ND2) has a left child      */
/* Perform a rotate between a node (ND2) and its left child             */
/* Update heights, then return new root -> Single Rotate With Left Node */

static position snglrotleft(position ND2)
 {
  position ND1;

  ND1 = ND2->left;
  ND2->left = ND1->right;
  ND1->right = ND2;

  return ND1;  /* New root */
 }

/* This function can be called only if node (ND1) has a right child      */
/* Perform a rotate between a node (ND1) and its right child             */
/* Update heights, then return new root -> Single Rotate With Right Node */

static position snglrotright(position ND1)
 {
  position ND2;

  ND2 = ND1->right;
  ND1->right = ND2->left;
  ND2->left = ND1;

  return ND2;  /* New root */
 }

treap insert(datatype item, treap T)
 {
  if(T == nullnode)
   {
    /* Create and return a one-node tree */
    T = malloc( sizeof( struct treap_node ) );
     if(!T) 
      {
      printf("Out of Memory\n");
      return NULL;
      }
    T->object = item; 
    T->prio = random_gen();
    T->left = T->right = nullnode;
   }
  if(GETKEY(item) < GETKEY(T->object))
    {
     T->left = insert( item, T->left );
     if(T->left->prio < T->prio) T = snglrotleft(T); 
    }
  if(GETKEY(item) > GETKEY(T->object))
    {
     T->right = insert(item, T->right);
     if(T->right->prio < T->prio)  T = snglrotright(T); 
    }

/* Otherwise it's a duplicate; do nothing */

 return T;
}

treap rm_node(datatype item, treap T)
 {
  if(T != nullnode)
  {
      if(GETKEY(item) < GETKEY(T->object)) 
    T->left = rm_node(item,T->left);
   else
       if(GETKEY(item) > GETKEY(T->object))
    T->right = rm_node(item,T->right); 
   else
   {
   /* Match found */
   T = (T->left->prio < T->right->prio) ? snglrotleft(T) : snglrotright(T);
   if(T != nullnode) T = rm_node(item, T);
   else
    {
     /* At a leaf */
     free(T->left); 
     T->left = nullnode;
    }
   }
  }
  return T; 
 }

datatype retrieve(position P)
 {
  return P->object;
 }
