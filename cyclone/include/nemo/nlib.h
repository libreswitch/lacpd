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

#ifndef __NLIB_H__
#define __NLIB_H__

/* various utility functions. The interface is remarkably like that of GLib
   (http://www.gtk.org), because I like GLib. I wrote prototypes for the GLib
   functions that seemed useful, then implemented the functions to that
   interface. A pseudo-cleanroom reimplementation of free software. Sigh.
*/

#ifdef __cplusplus
extern "C" {
#endif

#ifdef STARSHIP
#ifndef BASE_OFFSET
#define BASE_OFFSET(param, type, member) (type *) (((char *)param) - ((size_t) &(((type *)0)->member)))
#endif
#endif

extern int bitmap_debug;

/***********************************************************************/
/* convenient macros for bitwise arithmetic operators */

#define N_BIT_SET(f, b)             ((f) |= b)
#define N_BIT_RESET(f, b)           ((f) &= ~(b))
#define N_BIT_FLIP(f, b)            ((f) ^= (b))
#define N_BIT_TEST(f, b)            ((f) & (b))
#define N_BIT_MATCH(f, b)           (((f) & (b)) == (b))
#define N_BIT_COMPARE(f, b1, b2)    (((f) & (b1)) == b2)
#define N_BIT_MASK_MATCH(f, g, b)   (!(((f) ^ (g)) & (b)))
#define N_BYTE_MASK                 (0xFF)

/***********************************************************************/
/* moderately memory-efficient bitmap allocation routines */

struct NBitmap;
struct NBitmap_create_options {
    int no_alloc; // if 1, use _set+_clear instead of _alloc+_free
    int blocksize;
};

struct NBitmap *n_bitmap_create(unsigned int start, unsigned int size,
                                struct NBitmap_create_options *opts);
extern void n_bitmap_free(struct NBitmap *bitmap);

/* returns 1 for success, 0 for failure (no free bits) */
extern int n_bitmap_alloc_bit(struct NBitmap *bitmap, int *bit);

/* returns 1 for success, 0 for failure ('numbits' contiguous bits not free) */
extern int n_bitmap_alloc_n_bits(struct NBitmap *bitmap, int numbits,
                                 int *first_bit);
/* returns 1 for success, 0 for failure ('numbits' contiguous bits not free) */
extern int n_bitmap_alloc_n_bits_align(struct NBitmap *b, int align_bits,
                                       int *which);
/* returns 1 for success, 0 for failure (bit wasn't allocated) */
extern int n_bitmap_free_bit(struct NBitmap *bitmap, int bit);
extern int n_bitmap_numfree(struct NBitmap *bitmap);
extern void n_bitmap_set_bit(struct NBitmap *bitmap, int bit);
extern void n_bitmap_clear_bit(struct NBitmap *bitmap, int bit);
/* n_bitmap_test_bit can be used on both types */
extern int n_bitmap_test_bit(struct NBitmap *bitmap, int bit);
extern void n_bitmap_debug(struct NBitmap *b, char *debug, int opts);
extern void n_bitmap_printbits (unsigned char *bitmap, unsigned int blocknum,
                                unsigned int num_words);
extern int n_bitmap_find_last_set_bit(struct NBitmap *b, int bit);
extern int n_bitmap_free_n_bits(struct NBitmap *b, int which, int numbits);

extern int pid_open(char *process_name);
extern int pid_check(char *process_name);

/* This is for the location of servers such as the ML Nameserver, the
   Log Dispatcher and the Error Dispatcher */
struct Host {
    char *host_name;
    int   host_port;
};

extern int is_env_valid(char *env);
extern int get_name_and_port(char *env, struct Host *host);

extern const int BITS_IN_WORD;  /* In nlib/bitmap.c */

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

#ifdef STARSHIP
/*-----------------------------------------
 * Macros to traverse the NList:
 *----------------------------------------*/

#define NLIST_FOREACH(list, ptr)                      \
do {                                                  \
    struct NList *_elem_, *_elem_next_;               \
    _elem_ = list;                                    \
                                                      \
    while (_elem_) {                                  \
        _elem_next_ = _elem_->next;                   \
        ptr = (__typeof__ (ptr)) (_elem_->data);      \

        /* User code goes here. "ptr" is the element.
         * Use "break" to terminate loop early. It is
         * safe to "return" from the middle.
         */

#define NLIST_FOREACH_END(list, ptr)                  \
        if (_elem_next_ == list) {                    \
            break;                                    \
        }                                             \
        _elem_ = _elem_next_;                         \
    }                                                 \
} while(0);

#endif

/*
 *  Queue item structure
 */
struct qentry {
    struct qentry *next;
    void *data;
};

/*
 *  Queue container structure
 *
 *  This structure encapsulates a linked list of qentry items.
 */
struct NQueue {
    struct qentry *begin, *end;
};

struct NQueue *n_queue_init(void);
extern void n_queue_done(struct NQueue *queue);
extern int n_queue_push(struct NQueue *queue, void *data);
extern void *n_queue_pop(struct NQueue *queue);
extern int n_queue_size(struct NQueue *queue);
extern int nlog_empty_queue(int block);
extern int errlog_empty_queue(int block);

#define N_QUEUE_FOREACH(queue, ptr)                             \
{                                                               \
    struct qentry *n_queue_foreach_elem;                        \
    n_queue_foreach_elem = queue->begin;                        \
    while (1) {                                                 \
        if (!n_queue_foreach_elem)                              \
            break;                                              \
        ptr = (__typeof__(ptr))(n_queue_foreach_elem->data);
        /* User code goes here. "ptr" is the element. Use "break" to
           terminate loop early. It is safe to "return" from the middle. */

#define N_QUEUE_FOREACH_END(queue, ptr)                         \
        if (n_queue_foreach_elem == queue->end) {               \
            break;                                              \
        }                                                       \
        n_queue_foreach_elem = n_queue_foreach_elem->next;      \
    }                                                           \
}

typedef struct trace_metadata {
    char *data;
    int   bytes_written;
    int   bytes_left;
} trace_metadata_t;

#define LOG_MAGIC_COOKIE 0xDEADBEEF
typedef struct buffer_entry {
    int flags;
#define BUFFER_ENTRY_FLAG_COOKIE_SENT      (1<<0)
#define BUFFER_ENTRY_FLAG_LEN_SENT         (1<<1)
#define BUFFER_ENTRY_FLAG_CPUNUM_SENT      (1<<2)
#define BUFFER_ENTRY_FLAG_MDATA_SENT       (1<<3)
#define BUFFER_ENTRY_FLAG_COOKIE_READ      (1<<4)
#define BUFFER_ENTRY_FLAG_LEN_READ         (1<<5)
#define BUFFER_ENTRY_FLAG_CPUNUM_READ      (1<<6)
#define BUFFER_ENTRY_FLAG_MDATA_READ       (1<<7)
#define BUFFER_ENTRY_FLAG_NEXT_PKT_DROPPED (1<<8)
#define BUFFER_ENTRY_FLAG(be, f)           ((be)->flags & (f))
#define BUFFER_ENTRY_SET_FLAG(be, f)       ((be)->flags |= (f))
#define BUFFER_ENTRY_CLR_FLAG(be, f)       ((be)->flags &= ~(f))
#define BUFFER_ENTRY_CLR_FLAGS(be)         ((be)->flags = 0)
    unsigned short log;
#define BUFFER_ENTRY_LOG_DEVLOG 1
#define BUFFER_ENTRY_LOG_SYSLOG 2
#define BUFFER_ENTRY_LOG_NEMLOG 3
#define BUFFER_ENTRY_LOG_ERRLOG 4

    trace_metadata_t mdata;

    int fd;
    int index;
    char *text;
    int cpu_num;
    int len;
    int bytes_written;
    int bytes_left;
    int bytes_read;
    int bytes_expected;
    int try_count;
} buffer_entry_t;

/* NCompareFunc returns -1 if thing1 should sort before thing2 */
typedef int (*NCompareFunc)(void *thing1, void *thing2);
typedef int (*NMatchFunc)(void *thing, void *data);

/* should probably be made internal */
extern struct NList *n_list_alloc(void); // efficient allocator of elements
extern struct NList *n_list_append(struct NList *list, void *data);
extern struct NList *n_list_prepend(struct NList *list, void *data);
extern struct NList *n_list_insert(struct NList *list, void *data, int position);
extern void n_list_free(struct NList *element); // doesn't free *data for you

// returns 1 for success and 0 for failure
extern int n_list_insert_after(struct NList *prevelem, void *data);

// returns 1 for success and 0 for failure
// of course, list_to_insert is allowed to be an empty or singleton list
extern int n_list_insert_list_after(struct NList *prevelem,
                                    struct NList *list_to_insert);

extern struct NList *n_list_insert_sorted(struct NList *list, void *data,
                                          NCompareFunc func);

// Calls func(elem->data, data) for each element.
// Returns elem when func returns 1. Returns NULL if it never does.
extern struct NList *n_list_find_data(struct NList *list, NMatchFunc func,
                                      void *data);
// Returns elem when data == elem->data. Returns NULL if it never does.
extern struct NList *n_list_find_opaque_data(struct NList *list,
                                             void *data);
// remove the first element for which elem->data == data
extern struct NList *n_list_remove_data(struct NList *list, void *data);

// remove an element from the list
extern struct NList *n_list_remove_node(struct NList *list, struct NList *elem);

// n_list_free_list frees whole list, doesn't do *data
extern struct NList *n_list_free_list(struct NList *list);

// removes first item, puts data in *data
extern struct NList *n_list_pop(struct NList *list, void **data);

extern struct NList *n_list_nth(struct NList *list, int n);
extern void *n_list_nth_data(struct NList *list, int n);
extern int n_list_length(struct NList *list);

#ifdef STARSHIP
extern int n_list_isempty(struct NList *list);
extern struct NList *n_list_traverse_delete(struct NList *list,
                                            NMatchFunc    func,
                                            void         *data);
extern struct NList *n_list_traverse(struct NList *list, NMatchFunc func,
                                     void *data);
#endif /* STARSHIP */

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

/***********************************************************************/
#ifndef _KERNEL

/* n_logbuf: size-limited circular buffers of chunks of data. Each chunk has
   a declared size and a void * for data. The total of all the declared sizes
   will never rise above a per-logbuf maximum. The data object is opaque to
   the n_logbuf functions, so they can be arbitrarily complex (and larger or
   smaller than the declared size too). A user-supplied function is used to
   free these objects, which can be given an arbitrary data pointer. */

struct NLogbuf;

typedef void (*NLogbuf_free_func_t)(void *chunk, void *data);

struct NLogbuf_new_options {
    NLogbuf_free_func_t freefunc;
    void *freefunc_data;
    int is_text;
};

/* n_logbuf_new: create a struct NLogbuf. NAME is copied and used for debug
   purposes. The logbuf will never hold more than MAXSIZE bytes of data.
   Options:
    FREEFUNC is used to free the data when necessary, and defaults to
    something which calls plain old 'free()'.
    FREEFUNC_DATA is given to freefunc.
*/
extern struct NLogbuf *n_logbuf_new(char *name, int maxsize,
                                    struct NLogbuf_new_options *opts);
extern void n_logbuf_rename(struct NLogbuf *logbuf, char *newname);
extern void n_logbuf_set_maxsize(struct NLogbuf *logbuf, int maxsize);
/* Once you give a chunk of data to the logbuf, the logbuf owns that chunk.
   Do not reference that data afterwards, and do not give it a static buffer.
*/
extern void n_logbuf_add(struct NLogbuf *logbuf, void *data, int dsize);
extern int n_logbuf_size(struct NLogbuf *logbuf);
extern int n_logbuf_chunks(struct NLogbuf *logbuf);
/* extract chunk number WHICH (from 0 to n_logbuf_chunks()-1). Returns a
   pointer to the data and stuffs the chunksize in *sizeptr. The data pointer
   is to logbuf-owned memory: you may only use it until the next call to an
   n_logbuf function for this logbuf. */
extern void *n_logbuf_extract_chunk(struct NLogbuf *logbuf, int which,
                                    int *dsizeptr);
/* free(): release everything in the logbuf */
extern void n_logbuf_free(struct NLogbuf *logbuf);

/* trigger an upload of all eligible logbufs */
extern void n_logbuf_upload(void);

/* set program info */
extern void n_logbuf_set_proginfo(char *binary_name, char *appname, int instance,
                                  int cpunum);
extern void n_logbuf_init(char *name);

/***********************************************************************/

/* n_log: error/debug logging functions.
                  NOT TO BE USED FOR USER-VISIBLE TEXT!
   This is for development debug logging only.
*/

// Definitions for cleanup later
#ifdef STARSHIP

enum NLog_Severity {

    NLog_DEBUG,
    NLog_INFO,
    NLog_NOTIF,
    NLog_WARNING,   // default threshold for LOG/EMIT
    NLog_ERROR,
    NLog_CRITICAL,
    NLog_ALERT,
    NLog_EMERGENCY, // default threshold for FATAL

    NLog_MAX_SEVERITY_LEVELS,
};

#define NLog_SILLY     NLog_INFO
#define NLog_NOISE     NLog_INFO
#define NLog_CHATTER   NLog_INFO
#define NLog_UNUSUAL   NLog_NOTIF
#define NLog_HMM       NLog_WARNING
#define NLog_CURIOUS   NLog_WARNING
#define NLog_ODD       NLog_WARNING
#define NLog_STRANGE   NLog_WARNING
#define NLog_WEIRD     NLog_WARNING
#define NLog_BAD       NLog_ERROR
#define NLog_FATAL     NLog_EMERGENCY

#else // STARSHIP

enum NLog_Severity {
    /* 0 through 16 are available for even more verbose debug messages.
       Larger numbers mean more severe: 0 ought to represent nigh-unbearable
       quantities of debug output. There isn't anything special about using
       17-31 here, but the severity vectors currently have 32 entries in them
       (sized by using NLog_MAX_SEVERITY_LEVELS), so don't use 1000000 unless
       you want to consume lots of memory. */
    NLog_SILLY = 17,
    NLog_NOISE,
    NLog_CHATTER,
    NLog_INFO,
    NLog_DEBUG,

    NLog_UNUSUAL, // default threshold for LOG
    NLog_HMM,
    NLog_CURIOUS,
    NLog_ODD,
    NLog_STRANGE,
    NLog_WEIRD,

    NLog_WARNING, // default threshold for EMIT
    NLog_BAD,
    NLog_ERROR,

    NLog_FATAL, // default threshold for FATAL

    NLog_MAX_SEVERITY_LEVELS,
};
#endif /* STARSHIP */

extern const char *n_log_severity_level_names[];

enum {
    NLog_MAX_CATEGORIES = 32,
};

enum NLog_Vector_Type {
    NLog_VECTOR_COMPILE = 0, // still figuring out how to implement this
    NLog_VECTOR_LOG,
    NLog_VECTOR_EMIT,
    NLog_VECTOR_FATAL,
    NLog_MAX_VECTORS, // for internal use only
};

typedef struct fac_array {
    char name[100];
} fac_array_t;

struct NLog_Facility {
    char *name;
    int add_timestamp;
    int no_suppress;
    int no_new_line;
    unsigned int category_mask;
    int facnum;
    unsigned long long md5num;
    enum NLog_Severity vector[NLog_MAX_VECTORS][NLog_MAX_CATEGORIES];
};

struct NLog_facility_options {
    int add_timestamp; // print seconds-since-epoch next to the facility name
    int no_suppress;   // don't suppress the messages in this facility
    int no_new_line;
};

extern unsigned long long nlog_md5num(char *facname);

extern int n_log_facility_create(char *name, /* NAME is copied internally */
                                 struct NLog_facility_options *options);
extern void n_log_facility_rename(int facility, char *name);
extern void n_log_set_severity(int facility, enum NLog_Vector_Type type,
                               unsigned int category, enum NLog_Severity severity);
extern void n_log_set_severity_all(int facility, enum NLog_Vector_Type type,
                                   enum NLog_Severity severity);
extern void n_log_set_severity_some(int facility, enum NLog_Vector_Type type,
                                    enum NLog_Severity severity,
                                    unsigned int categories, ...);
#ifdef STARSHIP
extern void n_log_set_severity_by_mask(int facility, enum NLog_Vector_Type type,
                                       unsigned int category_mask,
                                       enum NLog_Severity severity);
extern enum NLog_Severity n_log_cnvsev(char *severity);
#endif

extern unsigned int n_log_num_to_bit_mask(int num);
extern int n_log_bit_mask_to_num(unsigned int bit_mask);
extern unsigned int n_log_cat_list_to_cat_mask(int list_size, ...);

extern struct NLog_Facility *n_log_get_facility_from_name(char *facility_name);
extern int n_log_get_facnum_from_name(char *facility_name);

/* n_log is the most general-purpose routine, and probably the least-used.
   You'll want to #define your own local versions to avoid giving the
   facility number over and over again. */
extern void n_log(int facility, unsigned int category, enum NLog_Severity severity,
                  char *fmt, ...) __attribute__ ((format (printf, 4, 5)));

extern void n_log_multiple(int facility, unsigned int category_mask,
                           enum NLog_Severity severity,
                           char *format, ...) __attribute__ ((format (printf, 4, 5)));
/* n_log_linefile and n_log_linefile_multiple just pretty-prepends the
   line/file/func text to the msg. */
extern void n_log_linefile(int facility, unsigned int category,
                           enum NLog_Severity severity, char *file,
                           int line, char *function, char *fmt, ...)
                           __attribute__ ((format (printf, 7, 8)));

extern void n_log_linefile_multiple(int facility, unsigned int category_mask,
                                    enum NLog_Severity severity,
                                    char *file, int line, char *function,
                                    char *fmt, ...)
                                    __attribute__ ((format (printf, 7, 8)));
#define n_log_line(facility, category, severity, fmt, args...)  \
    n_log_linefile(facility, category, severity,                \
                   __FILE__, __LINE__, __FUNCTION__,            \
                   fmt, ## args)
#define n_log_line_multiple(facility, category_mask, severity, fmt, args...)  \
    n_log_linefile_multiple(facility, category_mask, severity,                \
                   __FILE__, __LINE__, __FUNCTION__,                          \
                   fmt, ## args)

#define n_log_fatal(facility, fmt, args...) \
    printf(fmt, args)


#ifndef STARSHIP
#define n_assert(condition)                             \
  if (!(condition)) n_log_fatal(0, "Assertion '%s' failed.", #condition)
#else
#define n_assert(condition)                             \
  if (!(condition)) n_log_line(0, 1, NLog_DEBUG, "Assertion '%s' failed.", #condition)
#endif

// returns 1 if message should be generated
extern int n_log_generate(int facility, unsigned int category,
                          enum NLog_Severity severity);

struct NLog_handler_block {
    int facility;
    unsigned int category_mask;
    enum NLog_Severity severity;
    unsigned int categories;
    int vector[NLog_MAX_VECTORS][NLog_MAX_CATEGORIES]; // copy of the relevant thresholds
};
typedef void (*NLog_Handler)(struct NLog_handler_block *block,
                             char *text, int len);
extern void n_log_default_handler(struct NLog_handler_block *block,
                                  char *text, int len);
extern void n_log_set_handler(NLog_Handler handler);
extern void nlog_mlsend_init(void);
extern void errlog_mlsend_init(void);

#else

/*
 * Rate-limited version version of kernel log/printf function to help
 * ensure the CPU is not saturated while processing error/warning
 * messages---and doing little else.  The following macro should be
 * used when messages may be emitted at high rate, and where loss of
 * one or more messages is not deemed important.
 *
 * NB: access to the 64-bit counter softclock_ticks may not be
 * atomic on 32-bit systems, and may be subject to clock interrupts
 * occuring in the middle of the access.  We protect accesses to the
 * softclock_ticks variable at the expense of a bit more overhead.
 * We minimize the amount of time the clock interrupt is disabled.
 * It is probably safe to comment out locking out of the clock()
 * is we can afford to loose/generate a few more messages.
 *
 * TODO: We currently use printf() because messages are emitted on the line
 * card serial port AND is captured by gatherer correctly with the right
 * slot number.  Messsages processed through log() are emitted only to
 * /dev/klog (and not the serial port because the gatherer opened the
 * klog device) AND does not seem to have the right slot number
 * attached---due to a bug in the gatherer.  We should move to log()
 * as the system become more mature because it will not be blocked on
 * relatively slow serial I/O.
 *
 * TODO: The 'C' preprocessor does not allow the presence of a parameter
 * between _fmt_ and _args_ for some reason.
 *
 *     log (LOG_WARNING, __FUNCTION__ ": [%u msgs] " _fmt_,
 *         dropped_msgs + 1, ## _args_);
 */

/*
 * The following include is needed for the u_int64_t data type.
 * <sys/types.h> is included in nemo_os.h, but a number of source
 * files include nlib.h before nemo_os.h.
 */
#include <sys/types.h>

/*
 * The macro N_LOG_RATE_DFLT_INTVL specifies the default interval between
 * message emissions, and is specified in units of 1 second.
 */
#define N_LOG_RATE_DFLT_INTVL   5         /* 1 message every 5 seconds */

extern u_int64_t softclock_ticks;
extern int hz;
extern int n_log_force_print;

#define n_log_kern(_fmt_, _args_...)                                      \
    {                                                                     \
        static u_int64_t last_tick = 0;                                   \
        static u_int32_t dropped_msgs = 0;                                \
        int s;                                                            \
        s = splclock ();                                                  \
        if (n_log_force_print ||                                          \
            (softclock_ticks - last_tick) > (hz*N_LOG_RATE_DFLT_INTVL)) { \
            splx (s);                                                     \
            printf (__FUNCTION__ ": " _fmt_, ## _args_);                  \
            /* log (LOG_ERR, __FUNCTION__ ": " _fmt_, ## _args_); */      \
            if (dropped_msgs) {                                           \
                printf ("    [suppressed '%u' prior messages]\n",         \
                    dropped_msgs);                                        \
                /* log (LOG_ERR, "    suppressed '%u' prior messages\n",  \
                    dropped_msgs); */                                     \
            }                                                             \
            last_tick = softclock_ticks;                                  \
            dropped_msgs = 0;                                             \
        } else {                                                          \
            splx (s);                                                     \
            dropped_msgs++;                                               \
        }                                                                 \
    }

/*
 * Variant of n_log_kern() which permits messages to be emitted at
 * a caller-specified rate, up to 1 message every 100 milliseconds
 * unless the special value 0 is passed in.  If the rate value passed
 * in is 0, messages will will be emitted as fast as the 'hz' times
 * per second.  The variable 'hz' is typically set to the value
 * 100 (unless overriden by the config file).  
 */

#define N_LOG_RATE_1_MPS     10         /* 1 message per second */
#define N_LOG_RATE_10_MPS    1          /* 10 messages per second */
#define N_LOG_RATE_MAX       0          /* Max emission rate */

#define n_log_kern_rate(_n_x_100ms_, _fmt_, _args_...)                    \
    {                                                                     \
        static u_int64_t last_tick = 0;                                   \
        static u_int32_t dropped_msgs = 0;                                \
        int s;                                                            \
        s = splclock ();                                                  \
        if (n_log_force_print ||                                          \
            (softclock_ticks - last_tick) > ((hz/10) * (_n_x_100ms_))) {  \
            splx (s);                                                     \
            printf (__FUNCTION__ ": " _fmt_, ## _args_);                  \
            /* log (LOG_ERR, __FUNCTION__ ": " _fmt_, ## _args_); */      \
            if (dropped_msgs) {                                           \
                printf ("    [suppressed '%u' prior messages]\n",         \
                    dropped_msgs);                                        \
                /* log (LOG_ERR, "    suppressed '%u' prior messages\n",  \
                    dropped_msgs); */                                     \
            }                                                             \
            last_tick = softclock_ticks;                                  \
            dropped_msgs = 0;                                             \
        } else {                                                          \
            splx (s);                                                     \
            dropped_msgs++;                                               \
        }                                                                 \
    }

#endif /* _KERNEL  */

/***********************************************************************/

/* pre: s is a connected socket
 *      pointer to the buffer of n bytes
 * post: return number of bytes read
 * or on error return -1
 */
extern int read_stream(int s, char *buf, int n);

/* pre: s is a connected socket
 *      pointer to the buffer of n bytes
 * post: return number of bytes sent
 * or on error return -1
 */
extern int write_stream(int s, char *buf, int n);

extern void communicate(int sfd, int cfd);

extern void ch_get_version_from_filename(char *dirName, const char *prefix,
                                         const char *suffix,
                                         /* OUT */ char *verstr, int);
extern char *verSuffix;

#ifdef __cplusplus
}
#endif

#endif /* __NLIB_H__ */
