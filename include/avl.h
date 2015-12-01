/*
 * (c) Copyright 2015 Hewlett Packard Enterprise Development LP
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef __LACPD_AVL_H__
#define __LACPD_AVL_H__

typedef struct lacp_avl_node {
    struct lacp_avl_node *parent;
    struct lacp_avl_node *left;
    struct lacp_avl_node *right;
    short l_height;
    short r_height;
    void *self;
    void *key;
} lacp_avl_node_t;

typedef int(LACP_AVL_COMPARE)(void *, void *  );

/* AVL tree root. */
typedef struct lacp_avl_tree {
    LACP_AVL_COMPARE *compare;
    lacp_avl_node_t *root;
    lacp_avl_node_t *first;
    lacp_avl_node_t *last;
    unsigned int num_nodes;
} lacp_avl_tree_t;

/* AVL functions. */
extern void *lacp_avl_insert_or_find(lacp_avl_tree_t *, lacp_avl_node_t *);
extern void lacp_avl_delete(lacp_avl_tree_t *, lacp_avl_node_t *);
extern void *lacp_avl_find(lacp_avl_tree_t *, void *);
extern void *lacp_avl_find_or_find_next(lacp_avl_tree_t *, void *, int);
extern void *lacp_avl_next(lacp_avl_node_t *);
extern void *lacp_avl_prev(lacp_avl_node_t *);

/* AVL access macros. */
#define LACP_AVL_INIT_TREE(TREE, COMPARE)  (TREE).compare = &(COMPARE);  \
                                           (TREE).first = NULL;          \
                                           (TREE).last = NULL;           \
                                           (TREE).root = NULL;           \
                                           (TREE).num_nodes = 0

#define LACP_AVL_INIT_NODE(NODE, SELF, KEY) (NODE).parent = NULL;        \
                                            (NODE).left = NULL;          \
                                            (NODE).right = NULL;         \
                                            (NODE).self = (SELF);        \
                                            (NODE).key = (KEY);          \
                                            (NODE).l_height = -1;        \
                                            (NODE).r_height = -1;

/* Macro definitions. */
#define LACP_AVL_INSERT(TREE, NODE)         (lacp_avl_insert_or_find(&(TREE), &(NODE)) == NULL)
#define LACP_AVL_INSERT_OR_FIND(TREE, NODE) lacp_avl_insert_or_find(&(TREE), &(NODE))
#define LACP_AVL_DELETE(TREE, NODE)         lacp_avl_delete(&(TREE), &(NODE))
#define LACP_AVL_FIND(TREE, KEY)            lacp_avl_find(&(TREE), (KEY))
#define LACP_AVL_NEXT(NODE)                 lacp_avl_next(&(NODE))
#define LACP_AVL_PREV(NODE)                 lacp_avl_prev(&(NODE))
#define LACP_AVL_FIRST(TREE)                (((&(TREE))->first != NULL) ? (&(TREE))->first->self : NULL)
#define LACP_AVL_LAST(TREE)                 (((&(TREE))->last != NULL) ? (&(TREE))->last->self : NULL)
#define LACP_AVL_IN_TREE(NODE)              (((NODE).l_height != -1) && ((NODE).r_height != -1))
#define LACP_AVL_FIND_NEXT(TREE, KEY)       lacp_avl_find_or_find_next(&(TREE), (KEY), TRUE)

/*****************************************************************************/
/* Standard compare functions                                                */
/*****************************************************************************/
extern int lacp_compare_port_handle(void *, void *);

#endif /* __LACPD_AVL_H__ */
