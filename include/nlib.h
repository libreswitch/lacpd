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

#ifndef __NLIB_H__
#define __NLIB_H__

/* various utility functions. The interface is remarkably like that of GLib
   (http://www.gtk.org), because I like GLib. I wrote prototypes for the GLib
   functions that seemed useful, then implemented the functions to that
   interface. A pseudo-cleanroom reimplementation of free software. Sigh.
*/


/***********************************************************************/
/* doubly-linked lists */

/* void is an empty list
   list = n_list_append(list, foo);
   foo = n_list_nth_data(list, 0);
   if (list) foo = list->data; // first
   if (list) foo = list->prev->data; // last
*/

struct NList {
    void *data;
    struct NList *next;
    struct NList *prev;
};

/* NCompareFunc returns -1 if thing1 should sort before thing2 */
typedef int (*NCompareFunc)(void *thing1, void *thing2);
typedef int (*NMatchFunc)(void *thing, void *data);

/* should probably be made internal */
extern struct NList *n_list_alloc(void); // efficient allocator of elements
extern struct NList *n_list_append(struct NList *list, void *data);
extern struct NList *n_list_prepend(struct NList *list, void *data);
extern struct NList *n_list_insert(struct NList *list, void *data, int position);
extern void n_list_free(struct NList *element); // doesn't free *data for you

extern struct NList *n_list_insert_sorted(struct NList *list, void *data,
                                          NCompareFunc func);

// Calls func(elem->data, data) for each element.
// Returns elem when func returns 1. Returns NULL if it never does.
extern struct NList *n_list_find_data(struct NList *list, NMatchFunc func,
                                      void *data);
// remove the first element for which elem->data == data
extern struct NList *n_list_remove_data(struct NList *list, void *data);


extern struct NList *n_list_nth(struct NList *list, int n);
extern int n_list_length(struct NList *list);

#define N_LIST_NEXT(list) (((struct NList *) list)->next)
#define N_LIST_PREV(list) (((struct NList *) list)->prev)
#define N_LIST_ELEMENT(list) (((struct NList *) list)->data)

/* nlist iterator: do *not* modify the list while inside!. Use like this:

   N_LIST_FOREACH(list, data) {
       if (data == foo) do_something();
       if (data == last) break;
   } N_LIST_FOREACH_END(list, data);

   Using it any other way (leaving out the {}, or the trailing ";") will
   probably cause a syntax error.
*/

#define N_LIST_FOREACH(list, ptr)                               \
{                                                               \
    struct NList *n_list_foreach_elem;                          \
    n_list_foreach_elem = list;                                 \
    while (n_list_foreach_elem) {                               \
        ptr = (__typeof__(ptr))(n_list_foreach_elem->data);
        /* User code goes here. "ptr" is the element. Use "break" to
           terminate loop early. It is safe to "return" from the middle. */

#define N_LIST_FOREACH_END(list, ptr)                           \
        n_list_foreach_elem = n_list_foreach_elem->next;        \
        if (n_list_foreach_elem == list)                        \
            break;                                              \
    }                                                           \
}

#endif /* __NLIB_H__ */
