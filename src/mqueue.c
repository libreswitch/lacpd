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
 * mqueue.c
 *
 *   This is the main file for MsgLib Adaptation
 *   (for intra-process thread communication).
 *
 */

#include <stdlib.h>
#include <errno.h>

#include "mqueue.h"

int
mqueue_init(mqueue_t *queue)
{
    pthread_mutex_init(&(queue->q_mutex), NULL);

    queue->q_head.q_forw = &(queue->q_tail);
    queue->q_head.q_back = NULL;
    queue->q_head.q_data = NULL;
    queue->q_tail.q_forw = NULL;
    queue->q_tail.q_back = &(queue->q_head);
    queue->q_tail.q_data = NULL;

    /* Initialize semaphore to value zero and PSHARED zero. */
    if (sem_init(&(queue->q_avail), 0, 0) != 0) {
        return errno;
    }

    return 0;

} // mqueue_init

int
mqueue_send(mqueue_t *queue, void* data)
{
    qelem_t *new_elem;

    if ((NULL == queue) || (NULL == data)) {
        return EINVAL;
    }

    if ((new_elem = (qelem_t *) malloc(sizeof(qelem_t))) == NULL) {
        return ENOMEM;
    }

    new_elem->q_data = data;

    pthread_mutex_lock(&(queue->q_mutex));
    insque(new_elem, queue->q_tail.q_back);

    if (sem_post(&(queue->q_avail)) != 0) {
        pthread_mutex_unlock(&(queue->q_mutex));
        return errno;
    }
    pthread_mutex_unlock(&(queue->q_mutex));

    return 0;

} // mqueue_send

int
mqueue_wait(mqueue_t *queue, void **data)
{
    qelem_t *new_elem;

    if ((NULL == queue) || (NULL == data)) {
        return EINVAL;
    }

    // Block until a new event is available.
    sem_wait(&(queue->q_avail));

    pthread_mutex_lock(&(queue->q_mutex));
    new_elem = queue->q_head.q_forw;
    remque(queue->q_head.q_forw);
    pthread_mutex_unlock(&(queue->q_mutex));

    *data = new_elem->q_data;

    free(new_elem);

    return 0;

} // mqueue_wait
