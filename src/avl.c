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

#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>

#include <avl.h>
#include <pm_cmn.h>

#ifndef MIN
#define MIN(X, Y)   (((X)<(Y)) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X, Y)   (((X)>(Y)) ? (X) : (Y))
#endif

void lacp_avl_balance_tree(lacp_avl_tree_t *, lacp_avl_node_t *);
void lacp_avl_rebalance(lacp_avl_node_t **);
void lacp_avl_rotate_right(lacp_avl_node_t **);
void lacp_avl_rotate_left(lacp_avl_node_t **);
void lacp_avl_swap_right_most(lacp_avl_tree_t *, lacp_avl_node_t *, lacp_avl_node_t *);
void lacp_avl_swap_left_most(lacp_avl_tree_t *, lacp_avl_node_t *, lacp_avl_node_t *);

/**PROC+**********************************************************************/
/* Name:       lacp_avl_insert_or_find                                       */
/*                                                                           */
/* Purpose:   Insert the supplied node into the specified AVL tree if key    */
/*            does not already exist, otherwise returning the existing node  */
/*                                                                           */
/* Returns:   void *               - Pointer to existing entry if found    . */
/*                                       NULL if no such entry (implies node */
/*                                       successfully inserted)              */
/*                                                                           */
/* Params:    IN     tree              - a pointer to the AVL tree           */
/*            IN     node              - a pointer to the node to insert     */
/*                                                                           */
/* Operation: Scan down the tree looking for the insert point, going left    */
/*            if the insert key is less than the key in the tree and         */
/*            right if it is greater. When the insert point is found insert  */
/*            the new node and rebalance the tree if necessary. Return the   */
/*            existing entry instead, if found                               */
/*                                                                           */
/**PROC-**********************************************************************/
void *
lacp_avl_insert_or_find(lacp_avl_tree_t *tree, lacp_avl_node_t *node)
{
    /**************************************************************************/
    /* insert specified node into tree                                        */
    /**************************************************************************/
    lacp_avl_node_t *parent_node;
    int result;
    void *existing_entry = NULL;

    assert(!LACP_AVL_IN_TREE(*node));

    node->r_height = 0;
    node->l_height = 0;

    if (tree->root == NULL) {
        /**********************************************************************/
        /* tree is empty, so insert at root                                   */
        /**********************************************************************/
        tree->root  = node;
        tree->first = node;
        tree->last  = node;
        tree->num_nodes = 1;
        goto EXIT;
    }

    /**************************************************************************/
    /* scan down the tree looking for the appropriate insert point            */
    /**************************************************************************/
    parent_node = tree->root;
    while (parent_node != NULL) {
        /**********************************************************************/
        /* go left or right, depending on comparison                          */
        /**********************************************************************/
        result = tree->compare(node->key, parent_node->key);

        if (result > 0) {
            /******************************************************************/
            /* new key is greater than this node's key, so move down right    */
            /* subtree                                                        */
            /******************************************************************/
            if (parent_node->right == NULL) {
                /**************************************************************/
                /* right subtree is empty, so insert here                     */
                /**************************************************************/
                node->parent = parent_node;
                parent_node->right = node;
                parent_node->r_height = 1;
                if (parent_node == tree->last) {
                    /**********************************************************/
                    /* parent was the right-most node in the tree, so new     */
                    /* node is now right-most                                 */
                    /**********************************************************/
                    tree->last = node;
                }
                break;

            } else {
                /**************************************************************/
                /* right subtree is not empty                                 */
                /**************************************************************/
                parent_node = parent_node->right;
            }

        } else if (result < 0) {
            /******************************************************************/
            /* new key is less than this node's key, so move down left subtree*/
            /******************************************************************/
            if (parent_node->left == NULL) {
                /**************************************************************/
                /* left subtree is empty, so insert here                      */
                /**************************************************************/
                node->parent = parent_node;
                parent_node->left = node;
                parent_node->l_height = 1;
                if (parent_node == tree->first) {
                    /**********************************************************/
                    /* parent was the left-most node in the tree, so new node */
                    /* is now left-most                                       */
                    /**********************************************************/
                    tree->first = node;
                }
                break;
            } else {
                /**************************************************************/
                /* left subtree is not empty                                  */
                /**************************************************************/
                parent_node = parent_node->left;
            }

        } else {
            /******************************************************************/
            /* found a matching key, so get out now and return entry found    */
            /******************************************************************/
            existing_entry = parent_node->self;
            node->r_height = -1;
            node->l_height = -1;
            goto EXIT;
        }
    }

    /**************************************************************************/
    /* now rebalance the tree if necessary                                    */
    /**************************************************************************/
    lacp_avl_balance_tree(tree, parent_node);

    /**************************************************************************/
    /* Update the number of nodes in the tree                                 */
    /**************************************************************************/
    tree->num_nodes++;

EXIT:
    return existing_entry;

} /* lacp_avl_insert_or_find */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_delete                                               */
/*                                                                           */
/* Purpose:   Delete the specified node from the specified AVL tree          */
/*                                                                           */
/* Returns:   Nothing                                                        */
/*                                                                           */
/* Params:    IN     tree              - a pointer to the AVL tree           */
/*            IN     node              - a pointer to the node to delete     */
/*                                                                           */
/**PROC-**********************************************************************/
void
lacp_avl_delete(lacp_avl_tree_t *tree, lacp_avl_node_t *node)
{
    /**************************************************************************/
    /* delete specified node from tree                                        */
    /**************************************************************************/
    lacp_avl_node_t *replace_node;
    lacp_avl_node_t *parent_node;
    u_short new_height;

    assert(LACP_AVL_IN_TREE(*node));

    if ((node->left == NULL) &&
        (node->right == NULL)) {
        /*************************************************************************/
        /* barren node (no children), so just delete it                          */
        /*************************************************************************/
        replace_node = NULL;

        if (tree->first == node) {
            /***********************************************************************/
            /* node was first in tree, so replace it                               */
            /***********************************************************************/
            tree->first = node->parent;
        }

        if (tree->last == node) {
            /***********************************************************************/
            /* node was last in tree, so replace it                                */
            /***********************************************************************/
            tree->last = node->parent;
        }

    } else if (node->left == NULL) {
        /*************************************************************************/
        /* node has no left son, so replace with right son                       */
        /*************************************************************************/
        replace_node = node->right;

        if (tree->first == node) {
            /***********************************************************************/
            /* node was first in tree, so replace it                               */
            /***********************************************************************/
            tree->first = replace_node;
        }

    } else if (node->right == NULL) {
        /*************************************************************************/
        /* node has no right son, so replace with left son                       */
        /*************************************************************************/
        replace_node = node->left;

        if (tree->last == node) {
            /***********************************************************************/
            /* node was last in tree, so replace it                                */
            /***********************************************************************/
            tree->last = replace_node;
        }

    } else {
        /*************************************************************************/
        /* node has both left and right-sons                                     */
        /*************************************************************************/
        if (node->r_height > node->l_height) {
            /***********************************************************************/
            /* right subtree is higher than left subtree                           */
            /***********************************************************************/
            if (node->right->left == NULL) {
                /*********************************************************************/
                /* can replace node with right-son (since it has no left-son)        */
                /*********************************************************************/
                replace_node = node->right;
                replace_node->left = node->left;
                replace_node->left->parent = replace_node;
                replace_node->l_height = node->l_height;
            } else {
                /*********************************************************************/
                /* swap with left-most descendent of right subtree                   */
                /*********************************************************************/
                lacp_avl_swap_left_most(tree, node->right, node);
                replace_node = node->right;
            }
        } else {
            /***********************************************************************/
            /* left subtree is higher (or subtrees are of same height)             */
            /***********************************************************************/
            if (node->left->right == NULL) {
                /*********************************************************************/
                /* can replace node with left-son (since it has no right-son)        */
                /*********************************************************************/
                replace_node = node->left;
                replace_node->right = node->right;
                replace_node->right->parent = replace_node;
                replace_node->r_height = node->r_height;
            } else {
                /*********************************************************************/
                /* swap with right-most descendent of left subtree                   */
                /*********************************************************************/
                lacp_avl_swap_right_most(tree, node->left, node);
                replace_node = node->left;
            }
        }
    }

    /***************************************************************************/
    /* save parent node of deleted node                                        */
    /***************************************************************************/
    parent_node = node->parent;

    /***************************************************************************/
    /* reset deleted node                                                      */
    /***************************************************************************/
    node->parent = NULL;
    node->right = NULL;
    node->left = NULL;
    node->r_height = -1;
    node->l_height = -1;

    if (replace_node != NULL) {
        /*************************************************************************/
        /* fix-up parent pointer of replacement node, and calculate new height   */
        /* of subtree                                                            */
        /*************************************************************************/
        replace_node->parent = parent_node;
        new_height = (u_short)(1 + MAX(replace_node->l_height,
                                       replace_node->r_height));
    } else {
        /*************************************************************************/
        /* no replacement, so new height of subtree is zero                      */
        /*************************************************************************/
        new_height = 0;
    }

    if (parent_node != NULL) {
        /*************************************************************************/
        /* fixup parent node                                                     */
        /*************************************************************************/
        if (parent_node->right == node) {
            /***********************************************************************/
            /* node is right son of parent                                         */
            /***********************************************************************/
            parent_node->right = replace_node;
            parent_node->r_height = new_height;
        } else {
            /***********************************************************************/
            /* node is left son of parent                                          */
            /***********************************************************************/
            parent_node->left = replace_node;
            parent_node->l_height = new_height;
        }

        /*************************************************************************/
        /* now rebalance the tree (if necessary)                                 */
        /*************************************************************************/
        lacp_avl_balance_tree(tree, parent_node);

    } else {
        /*************************************************************************/
        /* replacement node is now root of tree                                  */
        /*************************************************************************/
        tree->root = replace_node;
    }

    /***************************************************************************/
    /* Update the number of nodes in the tree                                  */
    /***************************************************************************/
    tree->num_nodes--;

    return;

} /*  lacp_avl_delete */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_find                                                 */
/*                                                                           */
/* Purpose:   Find the node in the AVL tree with the supplied key            */
/*                                                                           */
/* Returns:   A pointer to the node                                          */
/*            NULL if no node is found with the specified key                */
/*                                                                           */
/* Params:    IN     tree              - a pointer to the AVL tree           */
/*            IN     key               - a pointer to the key                */
/*                                                                           */
/* Operation: Search down the tree going left if the search key is less than */
/*            the node in the tree and right if the search key is greater.   */
/*            When we run out of tree to search through either we've found   */
/*            it or the node is not in the tree.                             */
/*                                                                           */
/**PROC-**********************************************************************/
void *
lacp_avl_find(lacp_avl_tree_t *tree, void *key)
{
    /***************************************************************************/
    /* find node with specified key                                            */
    /***************************************************************************/
    lacp_avl_node_t *node;
    int result;

    node = tree->root;

    while (node != NULL) {
        /*************************************************************************/
        /* compare key of current node with supplied key                         */
        /*************************************************************************/
        result = tree->compare(key, node->key);

        if (result > 0) {
            /***********************************************************************/
            /* specified key is greater than key of this node, so look in right    */
            /* subtree                                                             */
            /***********************************************************************/
            node = node->right;

        } else if (result < 0) {
            /***********************************************************************/
            /* specified key is less than key of this node, so look in left        */
            /* subtree                                                             */
            /***********************************************************************/
            node = node->left;

        } else {
            /***********************************************************************/
            /* found the requested node                                            */
            /***********************************************************************/
            break;
        }
    }

    return ((node != NULL) ? node->self : NULL);

} /*  lacp_avl_find */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_next                                                 */
/*                                                                           */
/* Purpose:   Find next node in the AVL tree                                 */
/*                                                                           */
/* Returns:   A pointer to the next node in the tree                         */
/*                                                                           */
/* Params:    IN     node              - a pointer to the current node in    */
/*                                       the tree                            */
/*                                                                           */
/* Operation: If the specified node has a right-son then return the left-    */
/*            most son of this. Otherwise search back up until we find a     */
/*            node of which we are in the left sub-tree and return that.     */
/*                                                                           */
/**PROC-**********************************************************************/
void *
lacp_avl_next(lacp_avl_node_t *node)
{
    /***************************************************************************/
    /* find next node in tree                                                  */
    /***************************************************************************/

    assert(LACP_AVL_IN_TREE(*node));

    if (node->right != NULL) {
        /*************************************************************************/
        /* next node is left-most node in right subtree                          */
        /*************************************************************************/
        node = node->right;
        while (node->left != NULL) {
            node = node->left;
        }

    } else {
        /*************************************************************************/
        /* no right-son, so find a node of which we are in the left subtree      */
        /*************************************************************************/
        while (node != NULL) {
            if ((node->parent == NULL) ||
                (node->parent->left == node)) {
                node = node->parent;
                break;
            }
            node = node->parent;
        }
    }

    return ((node != NULL) ? node->self : NULL);

} /*  lacp_avl_next */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_prev                                                 */
/*                                                                           */
/* Purpose:   Find previous node in the AVL tree                             */
/*                                                                           */
/* Returns:   A pointer to the previous node in the tree                     */
/*                                                                           */
/* Params:    IN     node              - a pointer to the current node in    */
/*                                       the tree                            */
/*                                                                           */
/* Operation: If we have a left-son then the previous node is the right-most */
/*            son of this. Otherwise, look for a node of whom we are in the  */
/*            left subtree and return that.                                  */
/*                                                                           */
/**PROC-**********************************************************************/
void *
lacp_avl_prev(lacp_avl_node_t *node)
{
    /***************************************************************************/
	/* find previous node in tree                                              */
    /***************************************************************************/

    assert(LACP_AVL_IN_TREE(*node));

    if (node->left != NULL) {
        /*************************************************************************/
        /* previous node is right-most node in left subtree                      */
        /*************************************************************************/
        node = node->left;
        while (node->right != NULL) {
            node = node->right;
        }

    } else {
        /*************************************************************************/
        /* no left-son, so find a node of which we are in the right subtree      */
        /*************************************************************************/
        while (node != NULL) {
            if ((node->parent == NULL) ||
                (node->parent->right == node)) {
                node = node->parent;
                break;
            }
            node = node->parent;
        }
    }

    return ((node != NULL) ? node->self : NULL);

} /*  lacp_avl_prev */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_balance_tree                                         */
/*                                                                           */
/* Purpose:   Reblance the tree starting at the supplied node and ending at  */
/*            the root of the tree                                           */
/*                                                                           */
/* Returns:   Nothing                                                        */
/*                                                                           */
/* Params:    IN     tree              - a pointer to the AVL tree           */
/*            IN     node              - a pointer to the node to start      */
/*                                       balancing from                      */
/*                                                                           */
/**PROC-**********************************************************************/
void
lacp_avl_balance_tree(lacp_avl_tree_t *tree, lacp_avl_node_t *node)
{
    /***************************************************************************/
    /* balance the tree starting at the supplied node, and ending at the root  */
    /* of the tree                                                             */
    /***************************************************************************/
    while (node->parent != NULL) {
        /*************************************************************************/
        /* node has uneven balance, so may need to rebalance it                  */
        /*************************************************************************/

        if (node->parent->right == node) {
            /***********************************************************************/
            /* node is right-son of its parent                                     */
            /***********************************************************************/
            node = node->parent;
            lacp_avl_rebalance(&node->right);

            /***********************************************************************/
            /* now update the right height of the parent                           */
            /***********************************************************************/
            node->r_height = (u_short)(1 + MAX(node->right->r_height,
                                               node->right->l_height));
        } else {
            /***********************************************************************/
            /* node is left-son of its parent                                      */
            /***********************************************************************/
            node = node->parent;
            lacp_avl_rebalance(&node->left);

            /***********************************************************************/
            /* now update the left height of the parent                            */
            /***********************************************************************/
            node->l_height = (u_short)(1 + MAX(node->left->r_height,
                                               node->left->l_height));
        }
    }

    if (node->l_height != node->r_height) {
        /*************************************************************************/
        /* rebalance root node                                                   */
        /*************************************************************************/
        lacp_avl_rebalance(&tree->root);
    }

    return;

} /*  lacp_avl_balance_tree */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_rebalance                                            */
/*                                                                           */
/* Purpose:   Reblance a subtree of the AVL tree (if necessary)              */
/*                                                                           */
/* Returns:   Nothing                                                        */
/*                                                                           */
/* Params:    IN/OUT subtree           - a pointer to the subtree to         */
/*                                       rebalance                           */
/*                                                                           */
/**PROC-**********************************************************************/
void
lacp_avl_rebalance(lacp_avl_node_t **subtree)
{
    /***************************************************************************/
    /* Local data                                                              */
    /***************************************************************************/
    int moment;

    /***************************************************************************/
    /* rebalance a subtree of the AVL tree                                     */
    /***************************************************************************/

    /***************************************************************************/
    /* How unbalanced - don't want to recalculate                              */
    /***************************************************************************/
    moment = (*subtree)->r_height - (*subtree)->l_height;

    if (moment > 1) {
        /*************************************************************************/
        /* subtree is heavy on the right side                                    */
        /*************************************************************************/
        if ((*subtree)->right->l_height > (*subtree)->right->r_height) {
            /***********************************************************************/
            /* right subtree is heavier on left side, so must perform right        */
            /* rotation on this subtree to make it heavier on the right side       */
            /***********************************************************************/
            lacp_avl_rotate_right(&(*subtree)->right);
        }

        /*************************************************************************/
        /* now rotate the subtree left                                           */
        /*************************************************************************/
        lacp_avl_rotate_left(subtree);

    } else if (moment < -1) {
        /*************************************************************************/
        /* subtree is heavy on the left side                                     */
        /*************************************************************************/
        if ((*subtree)->left->r_height > (*subtree)->left->l_height) {
            /***********************************************************************/
            /* left subtree is heavier on right side, so must perform left         */
            /* rotation on this subtree to make it heavier on the left side        */
            /***********************************************************************/
            lacp_avl_rotate_left(&(*subtree)->left);
        }

        /*************************************************************************/
        /* now rotate the subtree right                                          */
        /*************************************************************************/
        lacp_avl_rotate_right(subtree);
    }

    return;

} /*  lacp_avl_rebalance */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_rotate_right                                         */
/*                                                                           */
/* Purpose:   Rotate a subtree of the AVL tree right                         */
/*                                                                           */
/* Returns:   Nothing                                                        */
/*                                                                           */
/* Params:    IN/OUT subtree           - a pointer to the subtree to rotate  */
/*                                                                           */
/**PROC-**********************************************************************/
void
lacp_avl_rotate_right(lacp_avl_node_t **subtree)
{
    /***************************************************************************/
    /* rotate subtree of AVL tree right                                        */
    /***************************************************************************/
    lacp_avl_node_t *left_son;

    left_son = (*subtree)->left;

    (*subtree)->left = left_son->right;
    if ((*subtree)->left != NULL) {
        (*subtree)->left->parent = (*subtree);
    }
    (*subtree)->l_height = left_son->r_height;

    left_son->parent = (*subtree)->parent;

    left_son->right = *subtree;
    left_son->right->parent = left_son;
    left_son->r_height = (u_short)(1 + MAX((*subtree)->r_height,
                                           (*subtree)->l_height));
    *subtree = left_son;

    return;

} /*  lacp_avl_rotate_right */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_rotate_left                                          */
/*                                                                           */
/* Purpose:   Rotate a subtree of the AVL tree left                          */
/*                                                                           */
/* Returns:   Nothing                                                        */
/*                                                                           */
/* Params:    IN/OUT subtree           - a pointer to the subtree to rotate  */
/*                                                                           */
/**PROC-**********************************************************************/
void
lacp_avl_rotate_left(lacp_avl_node_t **subtree)
{
    /***************************************************************************/
    /* rotate a subtree of the AVL tree left                                   */
    /***************************************************************************/
    lacp_avl_node_t *right_son;

    right_son = (*subtree)->right;

    (*subtree)->right = right_son->left;
    if ((*subtree)->right != NULL) {
        (*subtree)->right->parent = (*subtree);
    }
    (*subtree)->r_height = right_son->l_height;

    right_son->parent = (*subtree)->parent;

    right_son->left = *subtree;
    right_son->left->parent = right_son;
    right_son->l_height = (u_short)(1 + MAX((*subtree)->r_height,
                                            (*subtree)->l_height));
    *subtree = right_son;

    return;

} /*  lacp_avl_rotate_left */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_swap_right_most                                      */
/*                                                                           */
/* Purpose:   Swap node with right-most descendent of subtree                */
/*                                                                           */
/* Returns:   Nothing                                                        */
/*                                                                           */
/* Params:    IN     tree              - a pointer to the tree               */
/*            IN     subtree           - a pointer to the subtree            */
/*            IN     node              - a pointer to the node to swap       */
/*                                                                           */
/**PROC-**********************************************************************/
void
lacp_avl_swap_right_most(lacp_avl_tree_t *tree, lacp_avl_node_t *subtree,
                         lacp_avl_node_t *node)
{
    /***************************************************************************/
    /* swap node with right-most descendent of specified subtree               */
    /***************************************************************************/
    lacp_avl_node_t *swap_node;
    lacp_avl_node_t *swap_parent;
    lacp_avl_node_t *swap_left;

    assert(node->right != NULL);
    assert(node->left != NULL);

    /***************************************************************************/
    /* find right-most descendent of subtree                                   */
    /***************************************************************************/
    swap_node = subtree;
    while (swap_node->right != NULL) {
        swap_node = swap_node->right;
    }

    assert(swap_node->r_height == 0);
    assert(swap_node->l_height <= 1);

    /***************************************************************************/
    /* save parent and left-son of right-most descendent                       */
    /***************************************************************************/
    swap_parent = swap_node->parent;
    swap_left = swap_node->left;

    /***************************************************************************/
    /* move swap node to its new position                                      */
    /***************************************************************************/
    swap_node->parent = node->parent;
    swap_node->right = node->right;
    swap_node->left = node->left;
    swap_node->r_height = node->r_height;
    swap_node->l_height = node->l_height;
    swap_node->right->parent = swap_node;
    swap_node->left->parent = swap_node;

    if (node->parent == NULL) {
        /*************************************************************************/
        /* node is at root of tree                                               */
        /*************************************************************************/
        tree->root = swap_node;

    } else if (node->parent->right == node) {
        /*************************************************************************/
        /* node is right-son of parent                                           */
        /*************************************************************************/
        swap_node->parent->right = swap_node;

    } else {
        /*************************************************************************/
        /* node is left-son of parent                                            */
        /*************************************************************************/
        swap_node->parent->left = swap_node;
    }

    /***************************************************************************/
    /* move node to its new position                                           */
    /***************************************************************************/
    node->parent = swap_parent;
    node->right = NULL;
    node->left = swap_left;
    if (node->left != NULL) {
        node->left->parent = node;
        node->l_height = 1;
    } else {
        node->l_height = 0;
    }
    node->r_height = 0;
    node->parent->right = node;

    return;

} /*  lacp_avl_swap_right_most */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_swap_left_most                                       */
/*                                                                           */
/* Purpose:   Swap node with left-most descendent of subtree                 */
/*                                                                           */
/* Returns:   Nothing                                                        */
/*                                                                           */
/* Params:    IN     tree              - a pointer to the tree               */
/*            IN     subtree           - a pointer to the subtree            */
/*            IN     node              - a pointer to the node to swap       */
/*                                                                           */
/**PROC-**********************************************************************/
void
lacp_avl_swap_left_most(lacp_avl_tree_t *tree, lacp_avl_node_t *subtree,
                        lacp_avl_node_t *node)
{
    /***************************************************************************/
    /* swap node with left-most descendent of specified subtree                */
    /***************************************************************************/
    lacp_avl_node_t *swap_node;
    lacp_avl_node_t *swap_parent;
    lacp_avl_node_t *swap_right;

    assert(node->right != NULL);
    assert(node->left != NULL);

    /***************************************************************************/
    /* find left-most descendent of subtree                                    */
    /***************************************************************************/
    swap_node = subtree;
    while (swap_node->left != NULL) {
        swap_node = swap_node->left;
    }

    assert(swap_node->l_height == 0);
    assert(swap_node->r_height <= 1);

    /***************************************************************************/
    /* save parent and right-son of left-most descendent                       */
    /***************************************************************************/
    swap_parent = swap_node->parent;
    swap_right = swap_node->right;

    /***************************************************************************/
    /* move swap node to its new position                                      */
    /***************************************************************************/
    swap_node->parent = node->parent;
    swap_node->right = node->right;
    swap_node->left = node->left;
    swap_node->r_height = node->r_height;
    swap_node->l_height = node->l_height;
    swap_node->right->parent = swap_node;
    swap_node->left->parent = swap_node;

    if (node->parent == NULL) {
        /*************************************************************************/
        /* node is at root of tree                                               */
        /*************************************************************************/
        tree->root = swap_node;

    } else if (node->parent->right == node) {
        /*************************************************************************/
        /* node is right-son of parent                                           */
        /*************************************************************************/
        swap_node->parent->right = swap_node;

    } else {
        /*************************************************************************/
        /* node is left-son of parent                                            */
        /*************************************************************************/
        swap_node->parent->left = swap_node;
    }

    /***************************************************************************/
    /* move node to its new position                                           */
    /***************************************************************************/
    node->parent = swap_parent;
    node->right = swap_right;
    node->left = NULL;
    if (node->right != NULL) {
        node->right->parent = node;
        node->r_height = 1;
    } else {
        node->r_height = 0;
    }
    node->l_height = 0;
    node->parent->left = node;

    return;

} /*  lacp_avl_swap_left_most */

/**PROC+**********************************************************************/
/* Name:       lacp_avl_find_or_find_next                                    */
/*                                                                           */
/* Purpose:   Find the successor node to the supplied key in the AVL tree    */
/*                                                                           */
/* Returns:   A pointer to the node                                          */
/*            NULL if no successor node to the supplied key is found         */
/*                                                                           */
/* Params:    IN     tree         - a pointer to the AVL tree                */
/*            IN     key          - a pointer to the key                     */
/*            IN     not_equal    - TRUE return a node strictly > key        */
/*                                  FALSE return a node >= key               */
/*                                                                           */
/**PROC-**********************************************************************/
void *
lacp_avl_find_or_find_next(lacp_avl_tree_t *tree, void *key, int not_equal)
{
    lacp_avl_node_t *node;
    void *found_node = NULL;
    int result;

    node = tree->root;

    if (node != NULL) {
        /*************************************************************************/
        /* There is something in the tree                                        */
        /*************************************************************************/
        for(;;) {
            /***********************************************************************/
            /* compare key of current node with supplied key                       */
            /***********************************************************************/
            result = tree->compare(key, node->key);

            if (result > 0) {
                /*********************************************************************/
                /* specified key is greater than key of this node, so look in right  */
                /* subtree                                                           */
                /*********************************************************************/
                if (node->right == NULL) {
                    /*******************************************************************/
                    /* We've found the previous node - so we now need to find the      */
                    /* successor to this one.                                          */
                    /*******************************************************************/
                    found_node =  lacp_avl_next(node);
                    break;
                }
                node = node->right;

            } else if (result < 0) {
                /*********************************************************************/
                /* specified key is less than key of this node, so look in left      */
                /* subtree                                                           */
                /*********************************************************************/
                if (node->left == NULL) {
                    /******************************************************************/
                    /* We've found the next node so store and drop out                */
                    /******************************************************************/
                    found_node = node->self;
                    break;
                }
                node = node->left;

            } else {
                /*********************************************************************/
                /* found the requested node                                          */
                /*********************************************************************/
                if (not_equal) {
                    /*******************************************************************/
                    /* need to find the successor node to this node                    */
                    /*******************************************************************/
                    found_node =  lacp_avl_next(node);

                } else {
                    found_node = node->self;
                }
                break;
            }
        }
    }

    return (found_node);

} /*  lacp_avl_find_or_find_next */


/*****************************************************************************/
/* Standard compare functions                                                */
/*****************************************************************************/


/**PROC+**********************************************************************/
/* Name:      lacp_compare_port_handle                                       */
/*                                                                           */
/* Purpose:   Standard function for comparing port_handle_t                  */
/*                                                                           */
/* Returns:   -1 if aa < bb                                                  */
/*             0 if aa = bb                                                  */
/*             1 if aa > bb                                                  */
/*                                                                           */
/* Params:    IN  aa                                                         */
/*            IN  bb                                                         */
/*                                                                           */
/**PROC-**********************************************************************/
int
lacp_compare_port_handle(void *aa, void *bb)
{
    int ret_val;

    if (*((port_handle_t *)aa) < *((port_handle_t *)bb)) {
        ret_val = -1;
    } else if (*((port_handle_t *)aa) > *((port_handle_t *)bb)) {
        ret_val = 1;
    } else {
        ret_val = 0;
    }

    return (ret_val);

} /* lacp_compare_port_handle  */
