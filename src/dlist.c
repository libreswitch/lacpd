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

/*
 * dlist.c, double-linked list implementation
 *
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

#include <nlib.h>

typedef struct NList NList;

NList *
n_list_alloc(void)
{
    NList *pval;

    pval = (NList *)malloc(sizeof(NList));
    if (pval == NULL) {
        fprintf(stderr, "LACPd dlist - n_list_alloc failed!\n");
    }

    return pval;

} // n_list_alloc

void
n_list_free(NList *element)
{
    free(element);

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

    // No match found if we get here. Need to return NULL.
    return NULL;
}

static void
n_list_remove(NList *elem)
{
    elem->next->prev = elem->prev;
    elem->prev->next = elem->next;
    n_list_free(elem);

} // n_list_remove


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
