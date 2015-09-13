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

/*
 * dlist.c, double-linked list implementation
 *
 */

#include <nemo_os.h>
#include <nlib.h>

typedef struct NList NList;

NList *
n_list_alloc(void)
{
    NList *pval;

    pval = (NList *)malloc(sizeof(NList));
    if (pval == NULL) {
        fprintf(stderr, "Halon - n_list_alloc failed!\n");
    }

    return pval;

} // n_list_alloc

void
n_list_free(NList *element)
{
    NEMO_FREE(element);

} // n_list_free

NList *
n_list_append(NList *list, void *data)
{
    NList *elem = n_list_alloc();

    if (!elem) {
        return NULL;
    }

    if (!list) {
        elem->next = elem;
        elem->prev = elem;
        elem->data = data;
        return elem;
    }

    elem->data = data;
    elem->next = list;
    elem->prev = list->prev;
    list->prev->next = elem;
    list->prev = elem;

    return list;

} // n_list_append

int
n_list_insert_list_after(NList *prevelem, NList *list_to_insert)
{
    NList *nextelem = NULL;
    NList *head = NULL, *tail = NULL;

    if (prevelem == NULL) {
        return 0;
    }

    if (list_to_insert == NULL) {
        return 1;
    }

    // Both the list being inserted into and the list being
    // inserted contain at least one node.

    // Obtain some handles.
    nextelem = prevelem->next;
    head = list_to_insert; // head of list_to_insert
    tail = list_to_insert->prev; // last element in list_to_insert

    // Manipulate the links.
    prevelem->next = head;
    head->prev = prevelem;
    tail->next = nextelem;
    nextelem->prev = tail;

    return 1;

} // n_list_insert_list_after

int
n_list_insert_after(NList *prevelem, void *data)
{
    NList *elem = n_list_alloc();

    if (!elem) {
        return 0;
    }

    elem->data = data;
    elem->next = prevelem->next;
    elem->prev = prevelem;
    prevelem->next = elem;
    elem->next->prev = elem;

    return 1;

} // n_list_insert_after

NList *
n_list_prepend(NList *list, void *data)
{
    NList *elem = n_list_alloc();

    if (!elem) {
        return NULL;
    }

    if (!list) {
        elem->next = elem;
        elem->prev = elem;
        elem->data = data;
        return elem;
    }

    elem->data = data;
    elem->next = list;
    elem->prev = list->prev;
    list->prev->next = elem;
    list->prev = elem;

    return elem;

} // n_list_prepend

NList *
n_list_insert(NList *list, void *data, int position)
{
    NList *elem;
    int length;

    // If list is NULL, a list will be created.
    // If position < length of list, the data will be inserted prior to the
    // current element at that position.
    // If position > length of list, NULL will be returned and nothing will
    // be modified.

    length = n_list_length(list);
    if (position > length) {
        return NULL;
    }
    if ((!list) || (position == 0)) {
        return (n_list_prepend(list, data));
    }
    if (position == length) {
        return (n_list_append(list, data));
    }
    elem = n_list_nth(list, position);
    n_list_prepend(elem, data);

    return list;

} // n_list_insert

NList *
n_list_insert_sorted(NList *list, void *data, NCompareFunc func)
{
    NList *elem;

    if (!list) {
        return (n_list_prepend(list, data));
    }

    if ((*func)(list->data, data) > 0) {
        return(n_list_prepend(list, data));
    }

    elem = list->next;
    while (elem != list && (*func)(data, elem->data) >= 0) {
        elem = elem->next;
    }

    n_list_prepend(elem, data);

    return list;
}

#ifdef STARSHIP

/*----------------------------------------------------------
 * Wrapper to traverse the list and delete elem by elem.
 * func is called for every element, until the end of the
 * list is reached.
 *
 *  +--------------------------------------------------+
 * !! Only those elements for which the 'func' returns !!
 * !! non-zero value are deleted from the list.        !!
 *  +--------------------------------------------------+
 *
 * Returns:
 * Pointer to the modified list.
 *
 *--------------------------------------------------------*/
NList *
n_list_traverse_delete(NList *list, NMatchFunc func, void *data)
{
    NList *elem, *elem_next;
    elem = list;

    while (list) {
        elem_next = elem->next;
        if ((*func)(elem->data,data)) {
            // Actual delete happens here.
            list = n_list_remove_node(list, elem);
        }
        if (elem_next == list) {
            break;
        }
        elem = elem_next;
    }

    return list;

} // n_list_traverse_delete

/*--------------------------------------------------
 * Wrapper to traverse the list.
 * func is called for every element, until either
 * the end of list is reached OR func returns
 * non-zero value.
 *------------------------------------------------*/
NList *
n_list_traverse(NList *list, NMatchFunc func, void *data)
{
    NList *elem;
    elem = list;
    while (elem) {
        if ((*func)(elem->data,data)) {
            return elem;
        }
        elem = elem->next;
        if (elem == list) {
            break;
        }
    }

    return NULL;

} // n_list_traverse

NList *
n_list_find_data(NList *list, NMatchFunc func, void *data)
{
    return (n_list_traverse(list,func,data));

} // n_list_find_data

#else

NList *
n_list_find_data(NList *list, NMatchFunc func, void *data)
{
    NList *elem;

    elem = list;
    while (elem) {
        if ((*func)(elem->data,data)) {
            return elem;
        }
        elem = elem->next;
        if (elem == list) {
            break;
        }
    }

    // Halon - bug?!  No match found if we get here.
    //         Need to return NULL.
    return NULL;
}
#endif /* STARSHIP */

NList *
n_list_find_opaque_data(NList *list, void *data)
{
    NList *elem;

    elem = list;
    while (elem) {
        if (elem->data == data) {
            return elem;
        }
        elem = elem->next;
        if (elem == list) {
            break;
        }
    }

    return NULL;

} // n_list_find_opaque_data

static void
n_list_remove(NList *elem)
{
    elem->next->prev = elem->prev;
    elem->prev->next = elem->next;
    n_list_free(elem);

} // n_list_remove

NList *
n_list_remove_node(NList *list, NList *elem)
{
    NList *listnext = list->next;

    // Remove offending element.
    n_list_remove(elem);

    if (elem == list) { // deleted head of list
        if (list == listnext) { // was singleton list
            return NULL; // no list left!
        } else {
            return listnext;
        }
    } else { // head of list still safe
        return list;
    }

    return NULL; // not reached

} // n_list_remove_node

NList *
n_list_remove_data(NList *list, void *data)
{
    NList *elem;

    if (!list) {
        return list;
    }

    if (list->data == data) {
        elem = list->next;
        n_list_remove(list);
        if (elem == list) {
            return NULL; // that was the last item
        }
        return elem;
    }

    elem = list->next;
    while (elem != list && elem->data != data) {
        elem = elem->next;
    }

    if (elem->data == data) {
        n_list_remove(elem);
    }

    return list;

} // n_list_remove_data

NList *
n_list_free_list(NList *list)
{
    NList *here, *there;

    if (!list) {
        return NULL;
    }

    here = list;
    while (1) {
        there = here->next;
        n_list_free(here);
        here = there;
        if (here == list) {
            break;
        }
    }

    return NULL;

} // n_list_free_list

NList *
n_list_nth(NList *list, int n)
{
    NList *elem;
    int i;

    if (n == 0) {
        return list;
    }

    if (!list) {
        return list;
    }

    elem = list->next;
    if (elem == list) { // asking for elem 1 or >1, but list length=1
        return NULL;  // umm.. fatal instead? n > n_list_length
    }

    i = 1;
    while (i < n) {
        i++;
        elem = elem->next;
        if (elem == list) {
            return NULL; // umm.. fatal instead? n > n_list_length
        }
    }

    return elem;

} // n_list_nth

void *
n_list_nth_data(NList *list, int n)
{
    NList *elem = n_list_nth(list, n);

    if (elem) {
        return elem->data;
    }

    return NULL;

} // n_list_nth_data

int
n_list_length(NList *list)
{
    NList *elem;
    int i;

    if (!list) {
        return 0;
    }

    elem = list->next;
    i = 1;
    while (elem != list) {
        i++;
        elem = elem->next;
    }

    return i;

} // n_list_length

NList *
n_list_pop(NList *list, void **data)
{
    NList *new_list;

    if (!list) {
        return list;
    }

    if (data) {
        *data = list->data;
    }

    if (list->next == list) {
        n_list_remove(list);
        return NULL;
    }

    new_list = list->next;
    n_list_remove(list);

    return new_list;

} // n_list_pop

#ifdef STARSHIP
int
n_list_isempty(NList *list)
{
    return (list) ? FALSE : TRUE;

} // n_list_isempty
#endif
