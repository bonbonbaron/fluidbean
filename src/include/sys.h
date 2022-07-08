struct _fluid_timer_t;
#ifndef _SYS_H
#define _SYS_H

#include "fluidbean.h"

#if HAVE_MATH_H
#include <math.h>
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_OPENMP
#include <omp.h>
#endif

#if HAVE_IO_H
#include <io.h> // _open(), _close(), read(), write() on windows
#endif

#if HAVE_SIGNAL_H
#include <signal.h>
#endif

/** Integer types  */
#if HAVE_STDINT_H
#include <stdint.h>

#else

/* Assume GLIB types */
typedef gint8    int8_t;
typedef guint8   uint8_t;
typedef gint16   int16_t;
typedef guint16  uint16_t;
typedef gint32   int32_t;
typedef guint32  uint32_t;
typedef gint64   int64_t;
typedef guint64  uint64_t;
typedef guintptr uintptr_t;
typedef gintptr  intptr_t;

#endif

#if defined(WIN32) &&  HAVE_WINDOWS_H
#include <winsock2.h>
#include <ws2tcpip.h>	/* Provides also socklen_t */

/* WIN32 special defines */
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#ifdef _MSC_VER
#pragma warning(disable : 4244)
#pragma warning(disable : 4101)
#pragma warning(disable : 4305)
#pragma warning(disable : 4996)
#endif

#endif

/* Darwin special defines (taken from config_macosx.h) */
#ifdef DARWIN
# define MACINTOSH
# define __Types__
#endif

#ifdef LADSPA
#include <gmodule.h>
#endif

#include <glib/gstdio.h>

/**
 * Macro used for safely accessing a message from a GError and using a default
 * message if it is NULL.
 * @param err Pointer to a GError to access the message field of.
 * @return Message string
 */
#define gerror_message(err)  ((err) ? err->message : "No error details")

#ifdef WIN32
char* get_windows_error(void);
#endif

#define INLINE              inline

#define VERSION_CHECK(major, minor, patch) ((major<<16)|(minor<<8)|(patch))

/* Integer<->pointer conversion */
#define POINTER_TO_UINT(x)  ((unsigned int)(uintptr_t)(x))
#define UINT_TO_POINTER(x)  ((void *)(uintptr_t)(x))
#define POINTER_TO_INT(x)   ((signed int)(intptr_t)(x))
#define INT_TO_POINTER(x)   ((void *)(intptr_t)(x))

/* Endian detection */
#define IS_BIG_ENDIAN       (G_BYTE_ORDER == G_BIG_ENDIAN)

#define LE32TOH(x)          GINT32_FROM_LE(x)
#define LE16TOH(x)          GINT16_FROM_LE(x)

#if IS_BIG_ENDIAN
#define FOURCC(_a, _b, _c, _d) \
    (uint32_t)(((uint32_t)(_a) << 24) | ((uint32_t)(_b) << 16) | ((uint32_t)(_c) << 8) | (uint32_t)(_d))
#else
#define FOURCC(_a, _b, _c, _d) \
    (uint32_t)(((uint32_t)(_d) << 24) | ((uint32_t)(_c) << 16) | ((uint32_t)(_b) << 8) | (uint32_t)(_a)) 
#endif

#if defined(__OS2__)
#define INCL_DOS
#include <os2.h>

/* Define socklen_t if not provided */
#if !HAVE_SOCKLEN_T
typedef int socklen_t;
#endif
#endif

/**
    Time functions

 */

unsigned int curtime(void);
double utime(void);


/**
    Timers

 */

/* if the callback function returns 1 the timer will continue; if it
   returns 0 it will stop */
typedef int (*timer_callback_t)(void *data, unsigned int msec);

typedef struct _fluid_timer_t timer_t;

timer_t *new_timer(int msec, timer_callback_t callback,
                               void *data, int new_thread, int auto_destroy,
                               int high_priority);

void delete_timer(timer_t *timer);
int timer_join(timer_t *timer);
int timer_stop(timer_t *timer);
int timer_is_running(const timer_t *timer);
long timer_get_interval(const timer_t * timer);

// Macros to use for pre-processor if statements to test which Glib thread API we have (pre or post 2.32)
#define NEW_GLIB_THREAD_API   GLIB_CHECK_VERSION(2,32,0)
#define OLD_GLIB_THREAD_API  !GLIB_CHECK_VERSION(2,32,0)

/* Muteces */

#if NEW_GLIB_THREAD_API

/* glib 2.32 and newer */

/* Regular mutex */
typedef GMutex mutex_t;
#define MUTEX_INIT          { 0 }
#define mutex_init(_m)      g_mutex_init (&(_m))
#define mutex_destroy(_m)   g_mutex_clear (&(_m))
#define mutex_lock(_m)      g_mutex_lock(&(_m))
#define mutex_unlock(_m)    g_mutex_unlock(&(_m))

/* Recursive lock capable mutex */
typedef g_rec_mutex rec_mutex_t;
#define rec_mutex_init(_m)      g_rec_mutex_init(&(_m))
#define rec_mutex_destroy(_m)   g_rec_mutex_clear(&(_m))
#define rec_mutex_lock(_m)      g_rec_mutex_lock(&(_m))
#define rec_mutex_unlock(_m)    g_rec_mutex_unlock(&(_m))

/* Dynamically allocated mutex suitable for cond_t use */
typedef GMutex    cond_mutex_t;
#define cond_mutex_lock(m)        g_mutex_lock(m)
#define cond_mutex_unlock(m)      g_mutex_unlock(m)

static INLINE cond_mutex_t *
new_cond_mutex(void)
{
    GMutex *mutex;
    mutex = g_new(GMutex, 1);
    g_mutex_init(mutex);
    return (mutex);
}

static INLINE void
delete_cond_mutex(cond_mutex_t *m)
{
    return_if_fail(m != NULL);
    g_mutex_clear(m);
    g_free(m);
}

/* Thread condition signaling */
typedef GCond cond_t;
#define cond_signal(cond)         g_cond_signal(cond)
#define cond_broadcast(cond)      g_cond_broadcast(cond)
#define cond_wait(cond, mutex)    g_cond_wait(cond, mutex)

static INLINE cond_t *
new_cond(void)
{
    GCond *cond;
    cond = g_new(GCond, 1);
    g_cond_init(cond);
    return (cond);
}

static INLINE void
delete_cond(cond_t *cond)
{
    return_if_fail(cond != NULL);
    g_cond_clear(cond);
    g_free(cond);
}

/* Thread private data */

typedef GPrivate private_t;
#define private_init(_priv)                  memset (&_priv, 0, sizeof (_priv))
#define private_free(_priv)
#define private_get(_priv)                   g_private_get(&(_priv))
#define private_set(_priv, _data)            g_private_set(&(_priv), _data)

#else

/* glib prior to 2.32 */

/* Regular mutex */
typedef GStatic_mutex mutex_t;
#define MUTEX_INIT          G_STATIC_MUTEX_INIT
#define mutex_destroy(_m)   g_static_mutex_free(&(_m))
#define mutex_lock(_m)      g_static_mutex_lock(&(_m))
#define mutex_unlock(_m)    g_static_mutex_unlock(&(_m))

#define mutex_init(_m)      do { \
  if (!g_thread_supported ()) g_thread_init (NULL); \
  g_static_mutex_init (&(_m)); \
} while(0)

/* Recursive lock capable mutex */
typedef GStatic_rec_mutex rec_mutex_t;
#define rec_mutex_destroy(_m)   g_static_rec_mutex_free(&(_m))
#define rec_mutex_lock(_m)      g_static_rec_mutex_lock(&(_m))
#define rec_mutex_unlock(_m)    g_static_rec_mutex_unlock(&(_m))

#define rec_mutex_init(_m)      do { \
  if (!g_thread_supported ()) g_thread_init (NULL); \
  g_static_rec_mutex_init (&(_m)); \
} while(0)

/* Dynamically allocated mutex suitable for cond_t use */
typedef GMutex    cond_mutex_t;
#define delete_cond_mutex(m)      g_mutex_free(m)
#define cond_mutex_lock(m)        g_mutex_lock(m)
#define cond_mutex_unlock(m)      g_mutex_unlock(m)

static INLINE cond_mutex_t *
new_cond_mutex(void)
{
    if(!g_thread_supported())
    {
        g_thread_init(NULL);
    }

    return g_mutex_new();
}

/* Thread condition signaling */
typedef GCond cond_t;
cond_t *new_cond(void);
#define delete_cond(cond)         g_cond_free(cond)
#define cond_signal(cond)         g_cond_signal(cond)
#define cond_broadcast(cond)      g_cond_broadcast(cond)
#define cond_wait(cond, mutex)    g_cond_wait(cond, mutex)

/* Thread private data */
typedef GStatic_private private_t;
#define private_get(_priv)                   g_static_private_get(&(_priv))
#define private_set(_priv, _data)            g_static_private_set(&(_priv), _data, NULL)
#define private_free(_priv)                  g_static_private_free(&(_priv))

#define private_init(_priv)                  do { \
  if (!g_thread_supported ()) g_thread_init (NULL); \
  g_static_private_init (&(_priv)); \
} while(0)

#endif


/* Atomic operations */

#define atomic_int_inc(_pi) g_atomic_int_inc(_pi)
#define atomic_int_get(_pi) g_atomic_int_get(_pi)
#define atomic_int_set(_pi, _val) g_atomic_int_set(_pi, _val)
#define atomic_int_dec_and_test(_pi) g_atomic_int_dec_and_test(_pi)
#define atomic_int_compare_and_exchange(_pi, _old, _new) \
  g_atomic_int_compare_and_exchange(_pi, _old, _new)

#if GLIB_MAJOR_VERSION > 2 || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 30)
#define atomic_int_exchange_and_add(_pi, _add) \
  g_atomic_int_add(_pi, _add)
#define atomic_int_add(_pi, _add) \
  g_atomic_int_add(_pi, _add)
#else
#define atomic_int_exchange_and_add(_pi, _add) \
  g_atomic_int_exchange_and_add(_pi, _add)
#define atomic_int_add(_pi, _add) \
  g_atomic_int_exchange_and_add(_pi, _add)
#endif

#define atomic_pointer_get(_pp)           g_atomic_pointer_get(_pp)
#define atomic_pointer_set(_pp, val)      g_atomic_pointer_set(_pp, val)
#define atomic_pointer_compare_and_exchange(_pp, _old, _new) \
  g_atomic_pointer_compare_and_exchange(_pp, _old, _new)

static INLINE void
atomic_float_set(atomicFloatT *fptr, float val)
{
    int32_t ival;
    memcpy(&ival, &val, 4);
    atomic_int_set((atomic_int_t *)fptr, ival);
}

static INLINE float
atomic_float_get(atomicFloatT *fptr)
{
    int32_t ival;
    float fval;
    ival = atomic_int_get((atomic_int_t *)fptr);
    memcpy(&fval, &ival, 4);
    return fval;
}


/* Threads */

/* other thread implementations might change this for their needs */
typedef void *thread_return_t;
/* static return value for thread functions which requires a return value */
#define THREAD_RETURN_VALUE (NULL)

typedef GThread thread_t;
typedef thread_return_t (*thread_func_t)(void *data);

#define THREAD_ID_NULL            NULL                    /* A NULL "ID" value */
#define thread_id_t               GThread *               /* Data type for a thread ID */
#define thread_get_id()           g_thread_self()         /* Get unique "ID" for current thread */

thread_t *new_thread(const char *name, thread_func_t func, void *data,
                                 int prio_level, int detach);
void delete_thread(thread_t *thread);
void thread_self_set_prio(int prio_level);
int thread_join(thread_t *thread);

/* Dynamic Module Loading, currently only used by LADSPA subsystem */
#ifdef LADSPA

typedef GModule module_t;

#define module_open(_name)        g_module_open((_name), G_MODULE_BIND_LOCAL)
#define module_close(_mod)        g_module_close(_mod)
#define module_error()            g_module_error()
#define module_name(_mod)         g_module_name(_mod)
#define module_symbol(_mod, _name, _ptr) g_module_symbol((_mod), (_name), (_ptr))

#endif /* LADSPA */

/* Sockets and I/O */

#if defined(WIN32)
typedef SOCKET socket_t;
#else
typedef int socket_t;
#endif

/* The function should return 0 if no error occurred, non-zero
   otherwise. If the function return non-zero, the socket will be
   closed by the server. */
typedef int (*server_func_t)(void *data, socket_t client_socket, char *addr);

socket_t *new_server_socket(int port, server_func_t func, void *data);
void delete_server_socket(socket_t *sock);
int server_socket_join(socket_t *sock);
void socket_close(socket_t sock);
typedef int istream_t;
typedef int ostream_t;
istream_t socket_get_istream(socket_t sock);
ostream_t socket_get_ostream(socket_t sock);

/* File access */
#define stat(_filename, _statbuf)   g_stat((_filename), (_statbuf))
#if !GLIB_CHECK_VERSION(2, 26, 0)
    /* GStat_buf has not been introduced yet, manually typedef to what they had at that time:
     * https://github.com/GNOME/glib/blob/e7763678b56e3be073cc55d707a6e92fc2055ee0/glib/gstdio.h#L98-L115
     */
    #if defined(WIN32) || HAVE_WINDOWS_H // somehow reliably mock G_OS_WIN32??
        // Any effort from our side to reliably mock GStat_buf on Windows is in vain. E.g. glib-2.16 is broken as it uses struct stat rather than struct _stat32 on Win x86.
        // Disable it (the user has been warned by cmake).
        #undef stat
        #define stat(_filename, _statbuf)  (-1)
        typedef struct _fluid_stat_buf_t{int st_mtime;} stat_buf_t;
    #else
        /* posix, OS/2, etc. */
        typedef struct stat stat_buf_t;
    #endif
#else
typedef GStat_buf stat_buf_t;
#endif


/* Profiling */
#if WITH_PROFILING
/** profiling interface between Profiling command shell and Audio
    rendering  API (Profile_0004.pdf- 3.2.2)
*/

/*
  -----------------------------------------------------------------------------
  Shell task side |    Profiling interface              |  Audio task side
  -----------------------------------------------------------------------------
  profiling       |    Internal    |      |             |      Audio
  command   <---> |<-- profling -->| Data |<--macros -->| <--> rendering
  shell           |    API         |      |             |      API

*/

/* default parameters for shell command "prof_start" in sys.c */
#define PROFILE_DEFAULT_BANK 0       /* default bank */
#define PROFILE_DEFAULT_PROG 16      /* default prog (organ) */
#define PROFILE_FIRST_KEY 12         /* first key generated */
#define PROFILE_LAST_KEY 108         /* last key generated */
#define PROFILE_DEFAULT_VEL 64       /* default note velocity */
#define PROFILE_VOICE_ATTEN -0.04f   /* gain attenuation per voice (d_b) */


#define PROFILE_DEFAULT_PRINT 0      /* default print mode */
#define PROFILE_DEFAULT_N_PROF 1     /* default number of measures */
#define PROFILE_DEFAULT_DURATION 500 /* default duration (ms)  */


extern unsigned short profile_notes; /* number of generated notes */
extern unsigned char profile_bank;   /* bank,prog preset used by */
extern unsigned char profile_prog;   /* generated notes */
extern unsigned char profile_print;  /* print mode */

extern unsigned short profile_n_prof;/* number of measures */
extern unsigned short profile_dur;   /* measure duration in ms */
extern atomic_int_t profile_lock ; /* lock between multiple shell */
/**/

/*----------------------------------------------
  Internal profiling API (in sys.c)
-----------------------------------------------*/




/* For OS that implement <ENTER> key for profile cancellation:
 1) Adds #define PROFILE_CANCEL
 2) Adds the necessary code inside profile_is_cancel() see sys.c
*/
#if defined(WIN32)      /* Profile cancellation is supported for Windows */
#define PROFILE_CANCEL

#elif defined(__OS2__)  /* OS/2 specific stuff */
/* Profile cancellation isn't yet supported for OS2 */

#else   /* POSIX stuff */
#define PROFILE_CANCEL /* Profile cancellation is supported for linux */
#include <unistd.h> /* STDIN_FILENO */
#include <sys/select.h> /* select() */
#endif /* posix */

/* logging profiling data (used on synthesizer instance deletion) */
void profiling_print(void);

/*----------------------------------------------
  Profiling Data (in sys.c)
-----------------------------------------------*/
/** Profiling data. Keep track of min/avg/max values to profile a
    piece of code. */
typedef struct _fluid_profile_data_t
{
    const char *description;        /* name of the piece of code under profiling */
    double min, max, total;   /* duration (microsecond) */
    unsigned int count;       /* total count */
    unsigned int n_voices;    /* voices number */
    unsigned int n_samples;   /* audio samples number */
} profile_data_t;

enum
{
    /* commands/status  (profiling interface) */
    PROFILE_STOP,    /* command to stop a profiling measure */
    PROFILE_START,   /* command to start a profile measure */
    PROFILE_READY,   /* status to signal that a profiling measure has finished
	                    and ready to be printed */
    /*- State returned by profile_get_status() -*/
    /* between profiling commands and internal profiling API */
    PROFILE_RUNNING, /* a profiling measure is running */
    PROFILE_CANCELED,/* a profiling measure has been canceled */
};

/* Data interface */
extern unsigned char profile_status ;       /* command and status */
extern unsigned int profile_end_ticks;      /* ending position (in ticks) */
extern profile_data_t profile_data[]; /* Profiling data */

/*----------------------------------------------
  Probes macros
-----------------------------------------------*/
/** Macro to obtain a time reference used for the profiling */
#define profile_ref() utime()

/** Macro to create a variable and assign the current reference time for profiling.
 * So we don't get unused variable warnings when profiling is disabled. */
#define profile_ref_var(name)     double name = utime()

/**
 * Profile identifier numbers. List all the pieces of code you want to profile
 * here. Be sure to add an entry in the profile_data table in
 * sys.c
 */
enum
{
    PROF_WRITE,
    PROF_ONE_BLOCK,
    PROF_ONE_BLOCK_CLEAR,
    PROF_ONE_BLOCK_VOICE,
    PROF_ONE_BLOCK_VOICES,
    PROF_ONE_BLOCK_REVERB,
    PROF_ONE_BLOCK_CHORUS,
    PROF_VOICE_NOTE,
    PROF_VOICE_RELEASE,
    PROFILE_NBR	/* number of profile probes */
};
/** Those macros are used to calculate the min/avg/max. Needs a profile number, a
    time reference, the voices and samples number. */

/* local macro : acquiere data */
#define profile_data(_num, _ref, voices, samples)\
{\
	double _now = utime();\
	double _delta = _now - _ref;\
	profile_data[_num].min = _delta < profile_data[_num].min ?\
                                   _delta : profile_data[_num].min; \
	profile_data[_num].max = _delta > profile_data[_num].max ?\
                                   _delta : profile_data[_num].max;\
	profile_data[_num].total += _delta;\
	profile_data[_num].count++;\
	profile_data[_num].n_voices += voices;\
	profile_data[_num].n_samples += samples;\
	_ref = _now;\
}

/** Macro to collect data, called from inner functions inside audio
    rendering API */
#define profile(_num, _ref, voices, samples)\
{\
	if ( profile_status == PROFILE_START)\
	{	/* acquires data */\
		profile_data(_num, _ref, voices, samples)\
	}\
}

/** Macro to collect data, called from audio rendering API (write_xxxx()).
 This macro control profiling ending position (in ticks).
*/
#define profile_write(_num, _ref, voices, samples)\
{\
	if (profile_status == PROFILE_START)\
	{\
		/* acquires data first: must be done before checking that profile is
           finished to ensure at least one valid data sample.
		*/\
		profile_data(_num, _ref, voices, samples)\
		if (synth_get_ticks(synth) >= profile_end_ticks)\
		{\
			/* profiling is finished */\
			profile_status = PROFILE_READY;\
		}\
	}\
}

#else

/* No profiling */
#define profiling_print()
#define profile_ref()  0
#define profile_ref_var(name)
#define profile(_num,_ref,voices, samples)
#define profile_write(_num,_ref, voices, samples)
#endif /* WITH_PROFILING */

/**

    Memory locking

    Memory locking is used to avoid swapping of the large block of
    sample data.
 */

#if defined(HAVE_SYS_MMAN_H) && !defined(__OS2__)
#define mlock(_p,_n)      mlock(_p, _n)
#define munlock(_p,_n)    munlock(_p,_n)
#else
#define mlock(_p,_n)      0
#define munlock(_p,_n)
#endif


/**

    Floating point exceptions

    check_fpe() checks for "unnormalized numbers" and other
    exceptions of the floating point processsor.
*/
#ifdef FPE_CHECK
#define check_fpe(expl) check_fpe_i386(expl)
#define clear_fpe() clear_fpe_i386()
unsigned int check_fpe_i386(char *explanation_in_case_of_fpe);
void clear_fpe_i386(void);
#else
#define check_fpe(expl)
#define clear_fpe()
#endif


/* System control */
void msleep(unsigned int msecs);

/**
 * Advances the given \c ptr to the next \c alignment byte boundary.
 * Make sure you've allocated an extra of \c alignment bytes to avoid a buffer overflow.
 *
 * @note \c alignment must be a power of two
 * @return Returned pointer is guaranteed to be aligned to \c alignment boundary and in range \f[ ptr <= returned_ptr < ptr + alignment \f].
 */
static INLINE void *align_ptr(const void *ptr, unsigned int alignment)
{
    uintptr_t ptr_int = (uintptr_t)ptr;
    unsigned int offset = ptr_int & (alignment - 1);
    unsigned int add = (alignment - offset) & (alignment - 1); // advance the pointer to the next alignment boundary
    ptr_int += add;

    /* assert alignment is power of two */

    return (void *)ptr_int;
}

#define DEFAULT_ALIGNMENT (64U)

#endif /* _SYS_H */
