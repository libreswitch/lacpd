/*
 * Copyright (C) 2005-2015 Hewlett-Packard Development Company, L.P.
 * All Rights Reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License"); you may
 *   not use this file except in compliance with the License. You may obtain
 *   a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *   License for the specific language governing permissions and limitations
 *   under the License.
 */

#ifndef LAVL_INCLUDED
#define LAVL_INCLUDED

/**INC+***********************************************************************/
/* Header:    avl.h                                                       */
/*                                                                           */
/* Purpose:   AVL tree functions                                             */
/*                                                                           */
/*                                                                           */
/* $Revision: 1.2 $$Modtime::   Aug 24 1998 18:00:16   $*/
/**INC-***********************************************************************/

/**STRUCT+********************************************************************/
/* Structure: avl_node_t                                                     */
/*                                                                           */
/* Description: Node in an AVL tree.                                         */
/*****************************************************************************/


typedef struct nemo_avl_node
{
  struct nemo_avl_node *parent;
  struct nemo_avl_node *left;
  struct nemo_avl_node *right;
  short l_height;
  short r_height;
  void *self;
  void *key;
} nemo_avl_node_t;

/**STRUCT-********************************************************************/

/*****************************************************************************/
/* compare function                                                          */
/*****************************************************************************/
typedef int(NEMO_AVL_COMPARE)(void *, void *  );
/**STRUCT+********************************************************************/
/* Structure: nemo_avl_tree_t                                                       */
/*                                                                           */
/* Description: AVL tree root.                                               */
/*****************************************************************************/

typedef struct nemo_avl_tree

{
  NEMO_AVL_COMPARE *compare;
  nemo_avl_node_t *root;
  nemo_avl_node_t *first;
  nemo_avl_node_t *last;
  unsigned int    num_nodes;
} nemo_avl_tree_t;

/**STRUCT-********************************************************************/

/*****************************************************************************/
/* AVL functions                                                             */
/*****************************************************************************/
extern void *nemo_avl_insert_or_find(nemo_avl_tree_t  *,
                                            nemo_avl_node_t  *
                                            );
extern void nemo_avl_delete(nemo_avl_tree_t *, nemo_avl_node_t *  );
extern void *nemo_avl_find(nemo_avl_tree_t *, void *  );
extern void *nemo_avl_find_or_find_next(nemo_avl_tree_t *,
                                             void *, int  );
extern void *nemo_avl_next(nemo_avl_node_t *  );
extern void *nemo_avl_prev(nemo_avl_node_t *  );
extern unsigned int nemo_avl_get_num_nodes(nemo_avl_tree_t *);

/*****************************************************************************/
/* AVL access macros                                                         */
/* if DCL libraries are included they conflict with the NEMO_AVL_* defines   */
/* So in that case we undefine them                                          */
/*****************************************************************************/

#define NEMO_AVL_INIT_TREE(TREE, COMPARE)   (TREE).compare = &(COMPARE); \
                                       (TREE).first = NULL;              \
                                       (TREE).last = NULL;               \
                                       (TREE).root = NULL;               \
                                       (TREE).num_nodes = 0

#define NEMO_AVL_TREE_INITIALIZER(TREE, COMPARE)   { compare: COMPARE,   \
                                       first:NULL,                       \
                                       last:NULL,                        \
                                       root:NULL }                       \

#define NEMO_AVL_INIT_NODE(NODE, SELF, KEY) (NODE).parent = NULL;        \
                                       (NODE).left = NULL;               \
                                       (NODE).right = NULL;              \
                                       (NODE).self = (SELF);             \
                                       (NODE).key = (KEY);               \
                                       (NODE).l_height = -1;             \
                                       (NODE).r_height = -1;

/*****************************************************************************/
/* Macro definitions                                                         */
/*****************************************************************************/
#define NEMO_AVL_INSERT(TREE, NODE)                                                \
               (nemo_avl_insert_or_find(&(TREE), &(NODE)  ) == NULL)
#define NEMO_AVL_INSERT_OR_FIND(TREE, NODE)                                        \
               nemo_avl_insert_or_find(&(TREE), &(NODE)  )
#define NEMO_AVL_DELETE(TREE, NODE)     nemo_avl_delete(&(TREE), &(NODE)  )
#define NEMO_AVL_FIND(TREE, KEY)        nemo_avl_find(&(TREE), (KEY)  )
#define NEMO_AVL_NEXT(NODE)             nemo_avl_next(&(NODE)  )
#define NEMO_AVL_PREV(NODE)             nemo_avl_prev(&(NODE)  )
#define NEMO_AVL_FIRST(TREE)                                                       \
                   (((&(TREE))->first != NULL) ? (&(TREE))->first->self : NULL)

#define NEMO_AVL_LAST(TREE)                                                        \
                     (((&(TREE))->last != NULL) ? (&(TREE))->last->self : NULL)

#define NEMO_AVL_IN_TREE(NODE)  (((NODE).l_height != -1) && ((NODE).r_height != -1))

#define NEMO_AVL_FIND_NEXT(TREE, KEY)                                              \
                      nemo_avl_find_or_find_next(&(TREE), (KEY), TRUE  )

#define NEMO_AVL_FIND_OR_FIND_NEXT(TREE, KEY)                                      \
                     nemo_avl_find_or_find_next(&(TREE), (KEY), FALSE  )

#define NEMO_AVL_NODE_COUNT(TREE)  nemo_avl_get_num_nodes(&(TREE)) 

/*****************************************************************************/
/* Standard compare functions                                                */
/*****************************************************************************/
extern int nemo_compare_byte(void *, void *  );
extern int nemo_compare_short(void *, void *  );
extern int nemo_compare_ushort(void *, void *  );
extern int nemo_compare_long(void *, void *  );
extern int nemo_compare_ulong(void *, void *  );
extern int nemo_compare_int(void *, void * );
extern int nemo_compare_uint(void *, void * );
extern int nemo_compare_case_str(void *, void * );
extern int nemo_compare_str(void *, void * );
extern int nemo_compare_port_handle(void *, void *  );
extern int nemo_compare_sockaddr(void *, void *  );
extern int nemo_compare_inaddr(void *, void *  );
extern int nemo_compare_macaddr(void *, void *  );

#endif
