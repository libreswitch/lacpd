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

/***************************************************************************
 *  File         : nemo_os.h
 *  Description  : The file is used by modules that reside both in
 *                 userspace (i.e. in simcore) as well as in the real kernel
 ***************************************************************************/
#ifndef __NEMO_OS_H__
#define __NEMO_OS_H__

#ifndef   _KERNEL
/* user-space stuff (*bsd, linux, solaris) */

#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>

#include <nemo/nemo_types.h>

/* the n_assert() on the malloc return value will probably be removed in
   production code */
#define NEMO_MALLOC(space,cast,size)   { space = (cast) malloc(size); \
                                         n_assert(space != NULL); }
#define NEMO_FREE(x)                   free(x)
#define NEMO_BZERO(src,len)            bzero(src,len)
#define NEMO_BCOPY(src,dst,len)        bcopy((void *)(src), (void *)(dst), \
                                             (size_t)(len))
#define NEMO_STRCPY(dst,src)           strcpy(dst, src)
#define NEMO_STRNCPY(dst,src,n)        strncpy(dst, src, n)
#define NEMO_STRCMP(dst,src)           strcmp(dst,src)
#define NEMO_STRCASECMP(dst,src)       strcasecmp(dst,src)
#define KTBL_ENTER_CRITICAL()
#define KTBL_EXIT_CRITICAL()
#define NEMO_STRDUP(in)                strdup(in)
#define NEMO_ABORT()                   abort()

#endif /* _KERNEL */

/* compatibility */
#define NEM_MALLOC NEMO_MALLOC
#define NEM_FREE NEMO_FREE
#define NEM_BZERO NEMO_BZERO
#define NEM_BCOPY NEMO_BCOPY
#define NEM_STR_CPY NEMO_STRCPY
#define NEM_STR_CMP NEMO_STRCMP
#define NEM_STR_CASE_CMP NEMO_STRCASECMP
#define NEMO_HTONL64(x) htobe64(x)
#define NEMO_NTOHL64(x) be64toh(x)

#endif    /* _NEMO_OS_H */
