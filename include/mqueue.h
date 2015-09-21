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

#ifndef __MQUEUE_H__
#define __MQUEUE_H__

#include <pthread.h>
#include <search.h>
#include <semaphore.h>

typedef struct qelem {
    struct qelem   *q_forw;
    struct qelem   *q_back;
    void           *q_data;
} qelem_t;

typedef struct mqueue {
    qelem_t         q_head;
    qelem_t         q_tail;
    pthread_mutex_t q_mutex;
    sem_t           q_avail;
} mqueue_t;

extern int mqueue_init(mqueue_t *queue);
extern int mqueue_send(mqueue_t *queue, void *data);
extern int mqueue_wait(mqueue_t *queue, void **data);

#endif  /*  __MQUEUE_H__  */
