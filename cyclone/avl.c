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

#include <nemo/nemo_os.h>
#include <nemo/avl.h>
#include <nemo/pm/pm_cmn.h>
#include <sys/socket.h>

#ifndef MIN
#define MIN(X, Y)   (((X)<(Y)) ? (X) : (Y))
#endif

#ifndef MAX
#define MAX(X, Y)   (((X)>(Y)) ? (X) : (Y))
#endif

void nemo_avl_balance_tree(nemo_avl_tree_t *, nemo_avl_node_t *);
void nemo_avl_rebalance(nemo_avl_node_t **);
void nemo_avl_rotate_right(nemo_avl_node_t **);
void nemo_avl_rotate_left(nemo_avl_node_t **);
void nemo_avl_swap_right_most(nemo_avl_tree_t *, nemo_avl_node_t *, nemo_avl_node_t *);
void nemo_avl_swap_left_most(nemo_avl_tree_t *, nemo_avl_node_t *, nemo_avl_node_t *);

/**PROC+**********************************************************************/
/* Name:       nemo_avl_insert_or_find                                       */
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
nemo_avl_insert_or_find(nemo_avl_tree_t *tree, nemo_avl_node_t *node)
{
    /**************************************************************************/
    /* insert specified node into tree                                        */
    /**************************************************************************/
    nemo_avl_node_t *parent_node;
    int result;
    void *existing_entry = NULL;

    assert(!NEMO_AVL_IN_TREE(*node));

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
    nemo_avl_balance_tree(tree, parent_node);

    /**************************************************************************/
    /* Update the number of nodes in the tree                                 */
    /**************************************************************************/
    tree->num_nodes++;

EXIT:
    return existing_entry;

} /* nemo_avl_insert_or_find */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_delete                                               */
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
nemo_avl_delete(nemo_avl_tree_t *tree, nemo_avl_node_t *node)
{
    /**************************************************************************/
    /* delete specified node from tree                                        */
    /**************************************************************************/
    nemo_avl_node_t *replace_node;
    nemo_avl_node_t *parent_node;
    u_short new_height;

    assert(NEMO_AVL_IN_TREE(*node));

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
                nemo_avl_swap_left_most(tree, node->right, node);
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
                nemo_avl_swap_right_most(tree, node->left, node);
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
        nemo_avl_balance_tree(tree, parent_node);

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

} /*  nemo_avl_delete */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_find                                                 */
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
nemo_avl_find(nemo_avl_tree_t *tree, void *key)
{
    /***************************************************************************/
    /* find node with specified key                                            */
    /***************************************************************************/
    nemo_avl_node_t *node;
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

} /*  nemo_avl_find */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_next                                                 */
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
nemo_avl_next(nemo_avl_node_t *node)
{
    /***************************************************************************/
    /* find next node in tree                                                  */
    /***************************************************************************/

    assert(NEMO_AVL_IN_TREE(*node));

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

} /*  nemo_avl_next */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_prev                                                 */
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
nemo_avl_prev(nemo_avl_node_t *node)
{
    /***************************************************************************/
    /* find previous node in tree                                              */
    /***************************************************************************/

    assert(NEMO_AVL_IN_TREE(*node));

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

} /*  nemo_avl_prev */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_balance_tree                                         */
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
nemo_avl_balance_tree(nemo_avl_tree_t *tree, nemo_avl_node_t *node)
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
            nemo_avl_rebalance(&node->right);

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
            nemo_avl_rebalance(&node->left);

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
        nemo_avl_rebalance(&tree->root);
    }

    return;

} /*  nemo_avl_balance_tree */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_rebalance                                            */
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
nemo_avl_rebalance(nemo_avl_node_t **subtree)
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
            nemo_avl_rotate_right(&(*subtree)->right);
        }

        /*************************************************************************/
        /* now rotate the subtree left                                           */
        /*************************************************************************/
        nemo_avl_rotate_left(subtree);

    } else if (moment < -1) {
        /*************************************************************************/
        /* subtree is heavy on the left side                                     */
        /*************************************************************************/
        if ((*subtree)->left->r_height > (*subtree)->left->l_height) {
            /***********************************************************************/
            /* left subtree is heavier on right side, so must perform left         */
            /* rotation on this subtree to make it heavier on the left side        */
            /***********************************************************************/
            nemo_avl_rotate_left(&(*subtree)->left);
        }

        /*************************************************************************/
        /* now rotate the subtree right                                          */
        /*************************************************************************/
        nemo_avl_rotate_right(subtree);
    }

    return;

} /*  nemo_avl_rebalance */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_rotate_right                                         */
/*                                                                           */
/* Purpose:   Rotate a subtree of the AVL tree right                         */
/*                                                                           */
/* Returns:   Nothing                                                        */
/*                                                                           */
/* Params:    IN/OUT subtree           - a pointer to the subtree to rotate  */
/*                                                                           */
/**PROC-**********************************************************************/
void
nemo_avl_rotate_right(nemo_avl_node_t **subtree)
{
    /***************************************************************************/
    /* rotate subtree of AVL tree right                                        */
    /***************************************************************************/
    nemo_avl_node_t *left_son;

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

} /*  nemo_avl_rotate_right */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_rotate_left                                          */
/*                                                                           */
/* Purpose:   Rotate a subtree of the AVL tree left                          */
/*                                                                           */
/* Returns:   Nothing                                                        */
/*                                                                           */
/* Params:    IN/OUT subtree           - a pointer to the subtree to rotate  */
/*                                                                           */
/**PROC-**********************************************************************/
void
nemo_avl_rotate_left(nemo_avl_node_t **subtree)
{
    /***************************************************************************/
    /* rotate a subtree of the AVL tree left                                   */
    /***************************************************************************/
    nemo_avl_node_t *right_son;

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

} /*  nemo_avl_rotate_left */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_swap_right_most                                      */
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
nemo_avl_swap_right_most(nemo_avl_tree_t *tree, nemo_avl_node_t *subtree,
                         nemo_avl_node_t *node)
{
    /***************************************************************************/
    /* swap node with right-most descendent of specified subtree               */
    /***************************************************************************/
    nemo_avl_node_t *swap_node;
    nemo_avl_node_t *swap_parent;
    nemo_avl_node_t *swap_left;

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

} /*  nemo_avl_swap_right_most */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_swap_left_most                                       */
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
nemo_avl_swap_left_most(nemo_avl_tree_t *tree, nemo_avl_node_t *subtree,
                        nemo_avl_node_t *node)
{
    /***************************************************************************/
    /* swap node with left-most descendent of specified subtree                */
    /***************************************************************************/
    nemo_avl_node_t *swap_node;
    nemo_avl_node_t *swap_parent;
    nemo_avl_node_t *swap_right;

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

} /*  nemo_avl_swap_left_most */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_find_or_find_next                                    */
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
nemo_avl_find_or_find_next(nemo_avl_tree_t *tree, void *key, int not_equal)
{
    nemo_avl_node_t *node;
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
                    found_node =  nemo_avl_next(node);
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
                    found_node =  nemo_avl_next(node);

                } else {
                    found_node = node->self;
                }
                break;
            }
        }
    }

    return (found_node);

} /*  nemo_avl_find_or_find_next */

/**PROC+**********************************************************************/
/* Name:       nemo_avl_get_num_nodes                                        */
/*                                                                           */
/* Purpose:   Get the count of the nodes in this tree.                       */
/*                                                                           */
/* Returns:   The number of nodes in the tree                                */
/*                                                                           */
/* Params:    IN     tree         - a pointer to the AVL tree                */
/*                                                                           */
/**PROC-**********************************************************************/
unsigned int
nemo_avl_get_num_nodes(nemo_avl_tree_t *tree)
{
    return (tree->num_nodes);
}

/*****************************************************************************/
/* Standard compare functions                                                */
/*****************************************************************************/

/**PROC+**********************************************************************/
/* Name:      nemo_compare_byte                                              */
/*                                                                           */
/* Purpose:   Standard function for comparing u_chars                        */
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
nemo_compare_byte(void *aa, void *bb)
{
    int ret_val;

    if (*(u_char *)aa < *(u_char *)bb) {
        ret_val = -1;
    } else if (*(u_char *)aa > *(u_char *)bb) {
        ret_val = 1;
    } else {
        ret_val = 0;
    }

    return (ret_val);

} /* nemo_compare_byte */

/**PROC+**********************************************************************/
/* Name:      nemo_compare_short                                             */
/*                                                                           */
/* Purpose:   Standard function for comparing shorts                         */
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
nemo_compare_short(void *aa, void *bb)
{
    int ret_val;

    if (*(short *)aa < *(short *)bb) {
        ret_val = -1;
    } else if (*(short *)aa > *(short *)bb) {
        ret_val = 1;
    } else {
        ret_val = 0;
    }

    return (ret_val);

} /* nemo_compare_short */

/**PROC+**********************************************************************/
/* Name:      nemo_compare_ushort                                            */
/*                                                                           */
/* Purpose:   Standard function for comparing u_shorts                       */
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
nemo_compare_ushort(void *aa, void *bb)
{
    int ret_val;

    if (*(u_short *)aa < *(u_short *)bb) {
        ret_val = -1;
    } else if (*(u_short *)aa > *(u_short *)bb) {
        ret_val = 1;
    } else {
        ret_val = 0;
    }

    return (ret_val);

} /* nemo_compare_ushort */

/**PROC+**********************************************************************/
/* Name:      nemo_compare_long                                              */
/*                                                                           */
/* Purpose:   Standard function for comparing longs                          */
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
nemo_compare_long(void *aa, void *bb)
{
    int ret_val;

    if (*(long *)aa < *(long *)bb) {
        ret_val = -1;
    } else if (*(long *)aa > *(long *)bb) {
        ret_val = 1;
    } else {
        ret_val = 0;
    }

    return (ret_val);

} /* nemo_compare_long */

/**PROC+**********************************************************************/
/* Name:      nemo_compare_ulong                                             */
/*                                                                           */
/* Purpose:   Standard function for comparing u_longs                        */
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
nemo_compare_ulong(void *aa, void *bb)
{
    int ret_val;

    if (*(u_long *)aa < *(u_long *)bb) {
        ret_val = -1;
    } else if (*(u_long *)aa > *(u_long *)bb) {
        ret_val = 1;
    } else {
        ret_val = 0;
    }

    return (ret_val);

} /* nemo_compare_ulong */

/**PROC+**********************************************************************/
/* Name:      nemo_compare_int                                               */
/*                                                                           */
/* Purpose:   Standard function for comparing ints                           */
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
nemo_compare_int(void *aa, void *bb)
{
    int ret_val;

    if (*(int *)aa < *(int *)bb) {
        ret_val = -1;
    } else if (*(int *)aa > *(int *)bb) {
        ret_val = 1;
    } else {
        ret_val = 0;
    }

    return (ret_val);

} /* nemo_compare_int */

/**PROC+**********************************************************************/
/* Name:      nemo_compare_uint                                              */
/*                                                                           */
/* Purpose:   Standard function for comparing u_ints                         */
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
nemo_compare_uint(void *aa, void *bb)
{
    int ret_val;

    if (*(u_int *)aa < *(u_int *)bb) {
        ret_val = -1;
    } else if (*(u_int *)aa > *(u_int *)bb) {
        ret_val = 1;
    } else {
        ret_val = 0;
    }

    return (ret_val);

} /* nemo_compare_uint */

/**PROC+**********************************************************************/
/* Name:      nemo_compare_str                                               */
/*                                                                           */
/* Purpose:   Standard function for comparing strings case   sensitive       */
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
nemo_compare_str(void *aa, void *bb)
{
    int ret_val;

    ret_val = NEMO_STRCMP((char *)aa, (char *)bb);
    return (ret_val);

} /* nemo_compare_str */


/**PROC+**********************************************************************/
/* Name:      nemo_compare_case_str                                          */
/*                                                                           */
/* Purpose:   Standard function for comparing strings case insensitive       */
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
nemo_compare_case_str(void *aa, void *bb)
{
    int ret_val;

    if (aa == NULL && bb == NULL) {
        return 0;
    }

    if (aa == NULL) {
        return -1;
    }

    if (bb == NULL) {
        return 1;
    }

    ret_val = NEMO_STRCASECMP((char *)aa, (char *)bb);
    return (ret_val);

} /* nemo_compare_case_str */


/**PROC+**********************************************************************/
/* Name:      nemo_compare_port_handle                                       */
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
nemo_compare_port_handle(void *aa, void *bb)
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

} /* nemo_compare_port_handle  */

/**PROC+**********************************************************************/
/* Name:      nemo_compare_sockaddr                                          */
/*                                                                           */
/* Purpose:   Standard function for comparing sockaddr                       */
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
nemo_compare_sockaddr(void *aa, void *bb)
{
    int ret_val;
    char *aad = ((struct sockaddr *)aa)->sa_data;
    char *bbd = ((struct sockaddr *)bb)->sa_data;

    ret_val = nemo_compare_uint((void *)aad, (void *)bbd);
    return (ret_val);

} /* nemo_compare_sockaddr  */

/**PROC+**********************************************************************/
/* Name:      nemo_compare_macaddr                                           */
/*                                                                           */
/* Purpose:   Standard function for comparing MAC-Addresses                  */
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
nemo_compare_macaddr(void *aa, void  *bb)
{
    int ret_val;

    ret_val = memcmp(aa, bb, 6);
    return (ret_val);

} /* nemo_compare_macaddr  */

#ifndef _KERNEL
#include <netinet/in.h>

/**PROC+**********************************************************************/
/* Name:      nemo_compare_inaddr                                            */
/*                                                                           */
/* Purpose:   Standard function for comparing MAC-Addresses                  */
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
nemo_compare_inaddr(void *aa, void  *bb)
{
    struct in_addr *a1 = aa, *a2 = bb;

    if (a1->s_addr > a2->s_addr) {
        return 1;
    } else if (a1->s_addr < a2->s_addr) {
        return -1;
    }

    return 0;

} /* nemo_compare_inaddr */
#endif
