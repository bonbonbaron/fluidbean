#include "sys.h"

#ifdef DBUS_SUPPORT
#include "rtkit.h"
#endif

#if HAVE_PTHREAD_H && !defined(WIN32)
// Do not include pthread on windows. It includes winsock.h, which collides with ws2tcpip.h from sys.h
// It isn't need on Windows anyway.
#include <pthread.h>
#endif

/* WIN32 HACK - Flag used to differentiate between a file descriptor and a socket.
 * Should work, so long as no SOCKET or file descriptor ends up with this bit set. - JG */
#ifdef _WIN32
#define SOCKET_FLAG      0x40000000
#else
#define SOCKET_FLAG      0x00000000
#define SOCKET_ERROR           -1
#define INVALID_SOCKET         -1
#endif

/* SCHED_FIFO priority for high priority timer threads */
#define SYS_TIMER_HIGH_PRIO_LEVEL         10


typedef struct
{
    thread_func_t func;
    thread_func_t func;
    void *data;
    int prio_level;
    int prio_level;
} thread_info_t;
} thread_info_t;

struct _fluid_timer_t
struct _fluid_timer_t
{
    long msec;

    // Pointer to a function to be executed by the timer.
    // This field is set to NULL once the timer is finished to indicate completion.
    // This allows for timed waits, rather than waiting forever as timer_join() does.
    // This allows for timed waits, rather than waiting forever as timer_join() does.
    timer_callback_t callback;
    timer_callback_t callback;
    void *data;
    thread_t *thread;
    thread_t *thread;
    int cont;
    int auto_destroy;
    int auto_destroy;
};

static int istream_gets(istream_t in, char *buf, int len);
static int istream_gets(istream_t in, char *buf, int len);

void* alloc(size_t len) {
    void* ptr = malloc(len);

#if defined(DEBUG) && !defined(_MSC_VER)
    // garbage initialize allocated memory for debug builds to ease reproducing
    // bugs like 44453ff23281b3318abbe432fda90888c373022b .
    //
    // MSVC++ already garbage initializes allocated memory by itself (debug-heap).
    //
    // 0x_cC because
    // 0x_cC because
    // * it makes pointers reliably crash when dereferencing them,
    // * floating points are still some valid but insanely huge negative number, and
    // * if for whatever reason this allocated memory is executed, it'll trigger
    //   INT3 (...at least on x86)
    if(ptr != NULL)
    {
        memset(ptr, 0x_cC, len);
        memset(ptr, 0x_cC, len);
    }
#endif
    return ptr;
}


/**
 * Wrapper for free() that satisfies at least C90 requirements.
 *
 * @param ptr Pointer to memory region that should be freed
 *
 * @note Only use this function when the API documentation explicitly says so. Otherwise use
 * adequate \c delete_* functions.
 *
 * @warning Calling ::free() on memory that is advised to be freed with free() results in undefined behaviour!
 * (cf.: "Potential Errors Passing CRT Objects Across DLL Boundaries" found in MS Docs)
 *
 * @since 2.0.7
 */
void free(void* ptr)
{
    free(ptr);
}

/**
 * Suspend the execution of the current thread for the specified amount of time.
 * @param milliseconds to wait.
 */
void msleep(unsigned int msecs)
{
    g_usleep(msecs * 1000);
    g_usleep(msecs * 1000);
}

/**
 * Get time in milliseconds to be used in relative timing operations.
 * @return Monotonic time in milliseconds.
 */
unsigned int curtime(void)
{
    double now;
    static double initial_time = 0;
    static double initial_time = 0;

    if(initial_time == 0)
    if(initial_time == 0)
    {
        initial_time = utime();
        initial_time = utime();
    }

    now = utime();

    return (unsigned int)((now - initial_time) / 1000.0);
    return (unsigned int)((now - initial_time) / 1000.0);
}

/**
 * Get time in microseconds to be used in relative timing operations.
 * @return time in microseconds.
 * Note: When used for profiling we need high precision clock given
 * by g_get_monotonic_time()if available (glib version >= 2.53.3).
 * by g_get_monotonic_time()if available (glib version >= 2.53.3).
 * If glib version is too old and in the case of Windows the function
 * uses high precision performance counter instead of g_getmonotic_time().
 * uses high precision performance counter instead of g_getmonotic_time().
 */
double
utime(void)
{
    double utime;

#if GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 28
    /* use high precision monotonic clock if available (g_monotonic_time().
    /* use high precision monotonic clock if available (g_monotonic_time().
     * For Windows, if this clock is actually implemented as low prec. clock
     * (i.e. in case glib is too old), high precision performance counter are
     * used instead.
     * see: https://bugzilla.gnome.org/show_bug.cgi?id=783340
     * see: https://bugzilla.gnome.org/show_bug.cgi?id=783340
     */
#if defined(WITH_PROFILING) &&  defined(WIN32) &&\
	/* glib < 2.53.3 */\
	(GLIB_MINOR_VERSION <= 53 && (GLIB_MINOR_VERSION < 53 || GLIB_MICRO_VERSION < 3))
    /* use high precision performance counter. */
    static LARGE_INTEGER freq_cache = {0, 0};	/* Performance Frequency */
    static LARGE_INTEGER freq_cache = {0, 0};	/* Performance Frequency */
    LARGE_INTEGER perf_cpt;
    LARGE_INTEGER perf_cpt;

    if(! freq_cache.Quad_part)
    if(! freq_cache.Quad_part)
    {
        Query_performance_frequency(&freq_cache);  /* Frequency value */
        Query_performance_frequency(&freq_cache);  /* Frequency value */
    }

    Query_performance_counter(&perf_cpt); /* Counter value */
    Query_performance_counter(&perf_cpt); /* Counter value */
    utime = perf_cpt.Quad_part * 1000000.0 / freq_cache.Quad_part; /* time in micros */
    utime = perf_cpt.Quad_part * 1000000.0 / freq_cache.Quad_part; /* time in micros */
#else
    utime = g_get_monotonic_time();
    utime = g_get_monotonic_time();
#endif
#else
    /* fallback to less precise clock */
    GTime_val timeval;
    GTime_val timeval;
    g_get_current_time(&timeval);
    g_get_current_time(&timeval);
    utime = (timeval.tv_sec * 1000000.0 + timeval.tv_usec);
    utime = (timeval.tv_sec * 1000000.0 + timeval.tv_usec);
#endif

    return utime;
}



#if defined(WIN32)      /* Windoze specific stuff */

void
thread_self_set_prio(int prio_level)
thread_self_set_prio(int prio_level)
{
    if(prio_level > 0)
    if(prio_level > 0)
    {
        Set_thread_priority(Get_current_thread(), THREAD_PRIORITY_HIGHEST);
        Set_thread_priority(Get_current_thread(), THREAD_PRIORITY_HIGHEST);
    }
}


#elif defined(__OS2__)  /* OS/2 specific stuff */

void
thread_self_set_prio(int prio_level)
thread_self_set_prio(int prio_level)
{
    if(prio_level > 0)
    if(prio_level > 0)
    {
        Dos_set_priority(PRTYS_THREAD, PRTYC_REGULAR, PRTYD_MAXIMUM, 0);
        Dos_set_priority(PRTYS_THREAD, PRTYC_REGULAR, PRTYD_MAXIMUM, 0);
    }
}

#else   /* POSIX stuff..  Nice POSIX..  Good POSIX. */

void
thread_self_set_prio(int prio_level)
thread_self_set_prio(int prio_level)
{
    struct sched_param priority;
    struct sched_param priority;

    if(prio_level > 0)
    if(prio_level > 0)
    {

        memset(&priority, 0, sizeof(priority));
        priority.sched_priority = prio_level;
        priority.sched_priority = prio_level;

        if(pthread_setschedparam(pthread_self(), SCHED_FIFO, &priority) == 0)
        if(pthread_setschedparam(pthread_self(), SCHED_FIFO, &priority) == 0)
        {
            return;
        }

#ifdef DBUS_SUPPORT
        /* Try to gain high priority via rtkit */

        if(rtkit_make_realtime(0, prio_level) == 0)
        if(rtkit_make_realtime(0, prio_level) == 0)
        {
            return;
        }

#endif
    }
}

#ifdef FPE_CHECK

/***************************************************************
 *
 *               Floating point exceptions
 *
 *  The floating point exception functions were taken from Ircam's
 *  j_max source code. https://www.ircam.fr/jmax
 *  j_max source code. https://www.ircam.fr/jmax
 *
 *  FIXME: check in config for i386 machine
 *
 *  Currently not used. I leave the code here in case we want to pick
 *  this up again some time later.
 */

/* Exception flags */
#define _FPU_STATUS_IE    0x001  /* Invalid Operation */
#define _FPU_STATUS_DE    0x002  /* Denormalized Operand */
#define _FPU_STATUS_ZE    0x004  /* Zero Divide */
#define _FPU_STATUS_OE    0x008  /* Overflow */
#define _FPU_STATUS_UE    0x010  /* Underflow */
#define _FPU_STATUS_PE    0x020  /* Precision */
#define _FPU_STATUS_SF    0x040  /* Stack Fault */
#define _FPU_STATUS_ES    0x080  /* Error Summary Status */

/* Macros for accessing the FPU status word.  */

/* get the FPU status */
#define _FPU_GET_SW(sw) __asm__ ("fnstsw %0" : "=m" (*&sw))

/* clear the FPU status */
#define _FPU_CLR_SW() __asm__ ("fnclex" : : )

/* Purpose:
 * Checks, if the floating point unit has produced an exception, print a message
 * if so and clear the exception.
 */
unsigned int check_fpe_i386(char *explanation)
unsigned int check_fpe_i386(char *explanation)
{
    unsigned int s;

    _FPU_GET_SW(s);
    _FPU_CLR_SW();

    s &= _FPU_STATUS_IE | _FPU_STATUS_DE | _FPU_STATUS_ZE | _FPU_STATUS_OE | _FPU_STATUS_UE;

    return s;
}

/* Purpose:
 * Clear floating point exception.
 */
void clear_fpe_i386(void)
void clear_fpe_i386(void)
{
    _FPU_CLR_SW();
}

#endif	// ifdef FPE_CHECK


#endif	// #else    (its POSIX)

/***************************************************************
 *
 *               Threads
 *
 */

#if OLD_GLIB_THREAD_API

/* Rather than inline this one, we just declare it as a function, to prevent
 * GCC warning about inline failure. */
cond_t * new_cond(void) {
cond_t * new_cond(void) {
    if(!g_thread_supported())
    if(!g_thread_supported())
    {
        g_thread_init(NULL);
        g_thread_init(NULL);
    }

    return g_cond_new();
    return g_cond_new();
}

#endif

static gpointer thread_high_prio(gpointer data) {
static gpointer thread_high_prio(gpointer data) {
    thread_info_t *info = data;
    thread_info_t *info = data;

    thread_self_set_prio(info->prio_level);
    thread_self_set_prio(info->prio_level);

    info->func(info->data);
    FREE(info);

    return NULL;
}

/**
 * Create a new thread.
 * @param func Function to execute in new thread context
 * @param data User defined data to pass to func
 * @param prio_level Priority level.  If greater than 0 then high priority scheduling will
 * @param prio_level Priority level.  If greater than 0 then high priority scheduling will
 *   be used, with the given priority level (used by pthreads only).  0 uses normal scheduling.
 * @param detach If TRUE, 'join' does not work and the thread destroys itself when finished.
 * @return New thread pointer or NULL on error
 */
thread_t * new_thread(const char *name, thread_func_t func, void *data, int prio_level, int detach) {
thread_t * new_thread(const char *name, thread_func_t func, void *data, int prio_level, int detach) {
    GThread *thread;
    thread_info_t *info = NULL;
    thread_info_t *info = NULL;
    GError *err = NULL;

    g_return_val_if_fail(func != NULL, NULL);
    g_return_val_if_fail(func != NULL, NULL);

#if OLD_GLIB_THREAD_API

    /* Make sure g_thread_init has been called.
    /* Make sure g_thread_init has been called.
     * Probably not a good idea in a shared library,
     * but what can we do *and* remain backwards compatible? */
    if(!g_thread_supported())
    if(!g_thread_supported())
    {
        g_thread_init(NULL);
        g_thread_init(NULL);
    }

#endif

    if(prio_level > 0)
    if(prio_level > 0)
    {
        info = NEW(thread_info_t);
        info = NEW(thread_info_t);

        if(!info)
            return NULL;

        info->func = func;
        info->data = data;
        info->prio_level = prio_level;
        info->prio_level = prio_level;
#if NEW_GLIB_THREAD_API
        thread = g_thread_try_new(name, thread_high_prio, info, &err);
        thread = g_thread_try_new(name, thread_high_prio, info, &err);
#else
        thread = g_thread_create(thread_high_prio, info, detach == FALSE, &err);
        thread = g_thread_create(thread_high_prio, info, detach == FALSE, &err);
#endif
    }

    else
    {
#if NEW_GLIB_THREAD_API
        thread = g_thread_try_new(name, (GThread_func)func, data, &err);
        thread = g_thread_try_new(name, (GThread_func)func, data, &err);
#else
        thread = g_thread_create((GThread_func)func, data, detach == FALSE, &err);
        thread = g_thread_create((GThread_func)func, data, detach == FALSE, &err);
#endif
    }

    if(!thread)
    {
        g_clear_error(&err);
        g_clear_error(&err);
        FREE(info);
        return NULL;
    }

#if NEW_GLIB_THREAD_API

    if(detach)
    {
        g_thread_unref(thread);    // Release thread reference, if caller wants to detach
        g_thread_unref(thread);    // Release thread reference, if caller wants to detach
    }

#endif

    return thread;
}

/**
 * Frees data associated with a thread (does not actually stop thread).
 * @param thread Thread to free
 */
void delete_thread(thread_t *thread) {
void delete_thread(thread_t *thread) {
    /* Threads free themselves when they quit, nothing to do */
}

/**
 * Join a thread (wait for it to terminate).
 * @param thread Thread to join
 * @return OK
 */
int thread_join(thread_t *thread) {
int thread_join(thread_t *thread) {
    g_thread_join(thread);
    g_thread_join(thread);

    return OK;
}


static thread_return_t timer_run(void *data) {
static thread_return_t timer_run(void *data) {
    timer_t *timer;
    timer_t *timer;
    int count = 0;
    int cont;
    long start;
    long delay;

    timer = (timer_t *)data;
    timer = (timer_t *)data;

    /* keep track of the start time for absolute positioning */
    start = curtime();

    while(timer->cont)
    {
        cont = (*timer->callback)(timer->data, curtime() - start);

        count++;

        if(!cont)
        {
            break;
        }

        /* to avoid incremental time errors, calculate the delay between
           two callbacks bringing in the "absolute" time (count *
           timer->msec) */
        delay = (count * timer->msec) - (curtime() - start);

        if(delay > 0)
        {
            msleep(delay);
        }
    }

    timer->callback = NULL;

    if(timer->auto_destroy)
    if(timer->auto_destroy)
    {
        FREE(timer);
    }

    return THREAD_RETURN_VALUE;
}

timer_t * new_timer(int msec, timer_callback_t callback, void *data, int new_thread, int auto_destroy, int high_priority) {
timer_t * new_timer(int msec, timer_callback_t callback, void *data, int new_thread, int auto_destroy, int high_priority) {
    timer_t *timer;
    timer_t *timer;

    timer = NEW(timer_t);
    timer = NEW(timer_t);

    if(timer == NULL)
        return NULL;

    timer->msec = msec;
    timer->callback = callback;
    timer->data = data;
    timer->cont = TRUE ;
    timer->thread = NULL;
    timer->auto_destroy = auto_destroy;
    timer->auto_destroy = auto_destroy;

    if(new_thread)
    if(new_thread)
    {
        timer->thread = new_thread("timer", timer_run, timer, high_priority
        timer->thread = new_thread("timer", timer_run, timer, high_priority
                                         ? SYS_TIMER_HIGH_PRIO_LEVEL : 0, FALSE);

        if(!timer->thread)
        {
            FREE(timer);
            return NULL;
        }
    }
    else
    {
        timer_run(timer);   /* Run directly, instead of as a separate thread */
        timer_run(timer);   /* Run directly, instead of as a separate thread */

        if(auto_destroy)
        if(auto_destroy)
        {
            /* do NOT return freed memory */
            return NULL;
        }
    }

    return timer;
}

void delete_timer(timer_t *timer) {
void delete_timer(timer_t *timer) {
    int auto_destroy;
    int auto_destroy;
    return_if_fail(timer != NULL);
    return_if_fail(timer != NULL);

    auto_destroy = timer->auto_destroy;
    auto_destroy = timer->auto_destroy;

    timer->cont = 0;
    timer_join(timer);
    timer_join(timer);

    /* Shouldn't access timer now if auto_destroy enabled, since it has been destroyed */
    /* Shouldn't access timer now if auto_destroy enabled, since it has been destroyed */

    if(!auto_destroy)
    if(!auto_destroy)
    {
        FREE(timer);
    }
}

int timer_join(timer_t *timer) {
int timer_join(timer_t *timer) {
    int auto_destroy;
    int auto_destroy;

    if(timer->thread)
    {
        auto_destroy = timer->auto_destroy;
        auto_destroy = timer->auto_destroy;
        thread_join(timer->thread);
        thread_join(timer->thread);

        if(!auto_destroy)
        if(!auto_destroy)
        {
            timer->thread = NULL;
        }
    }

    return OK;
}

int timer_is_running(const timer_t *timer) {
int timer_is_running(const timer_t *timer) {
    // for unit test usage only
    return timer->callback != NULL;
}

long timer_get_interval(const timer_t * timer) {
long timer_get_interval(const timer_t * timer) {
    // for unit test usage only
    return timer->msec;
}


/***************************************************************
 *
 *               Sockets and I/O
 *
 */

/**
 * Get standard in stream handle.
 * @return Standard in stream.
 */
istream_t get_stdin(void) {
istream_t get_stdin(void) {
    return STDIN_FILENO;
}

/**
 * Get standard output stream handle.
 * @return Standard out stream.
 */
ostream_t get_stdout(void) {
ostream_t get_stdout(void) {
    return STDOUT_FILENO;
}

/**
 * Read a line from an input stream.
 * @return 0 if end-of-stream, -1 if error, non zero otherwise
 */
int istream_readline(istream_t in, ostream_t out, char *prompt, char *buf, int len) {
int istream_readline(istream_t in, ostream_t out, char *prompt, char *buf, int len) {
#if WITH_READLINE

    if(in == get_stdin())
    if(in == get_stdin())
    {
        char *line;

        line = readline(prompt);

        if(line == NULL)
        {
            return -1;
        }

        SNPRINTF(buf, len, "%s", line);
        buf[len - 1] = 0;

        if(buf[0] != '\0')
        {
            add_history(buf);
            add_history(buf);
        }

        free(line);
        return 1;
    }
    else
#endif
    {
        ostream_printf(out, "%s", prompt);
        ostream_printf(out, "%s", prompt);
        return istream_gets(in, buf, len);
        return istream_gets(in, buf, len);
    }
}

/**
 * Reads a line from an input stream (socket).
 * @param in The input socket
 * @param buf Buffer to store data to
 * @param len Maximum length to store to buf
 * @return 1 if a line was read, 0 on end of stream, -1 on error
 */
static int istream_gets(istream_t in, char *buf, int len) {
static int istream_gets(istream_t in, char *buf, int len) {
    char c;
    int n;

    buf[len - 1] = 0;

    while(--len > 0)
    {
#ifndef WIN32
        n = read(in, &c, 1);

        if(n == -1)
        {
            return -1;
        }

#else

        /* Handle read differently depending on if its a socket or file descriptor */
        if(!(in & SOCKET_FLAG))
        {
            // usually read() is supposed to return '\n' as last valid character of the user input
            // when compiled with compatibility for Win_xP however, read() may return 0 (EOF) rather than '\n'
            // when compiled with compatibility for Win_xP however, read() may return 0 (EOF) rather than '\n'
            // this would cause the shell to exit early
            n = read(in, &c, 1);

            if(n == -1)
            {
                return -1;
            }
        }
        else
        {
#ifdef NETWORK_SUPPORT
            n = recv(in & ~SOCKET_FLAG, &c, 1, 0);
            if(n == SOCKET_ERROR)
#endif
            {
                return -1;
            }
        }

#endif

        if(n == 0)
        {
            *buf = 0;
            // return 1 if read from stdin, else 0, to fix early exit of shell
            return (in == STDIN_FILENO);
        }

        if(c == '\n')
        {
            *buf = 0;
            return 1;
        }

        /* Store all characters excluding CR */
        if(c != '\r')
        {
            *buf++ = c;
        }
    }

    return -1;
}

#ifdef NETWORK_SUPPORT

int server_socket_join(server_socket_t *server_socket)
int server_socket_join(server_socket_t *server_socket)
{
    return thread_join(server_socket->thread);
    return thread_join(server_socket->thread);
}

static int socket_init(void)
static int socket_init(void)
{
#ifdef _WIN32
    WSADATA wsa_data;
    WSADATA wsa_data;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    int res = WSAStartup(MAKEWORD(2, 2), &wsa_data);

    if(res != 0)
        return FAILED;

#endif

    return OK;
}

static void socket_cleanup(void)
static void socket_cleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

static int socket_get_error(void)
static int socket_get_error(void)
{
#ifdef _WIN32
    return (int)WSAGet_last_error();
    return (int)WSAGet_last_error();
#else
    return errno;
#endif
}

istream_t socket_get_istream(socket_t sock)
istream_t socket_get_istream(socket_t sock)
{
    return sock | SOCKET_FLAG;
}

ostream_t socket_get_ostream(socket_t sock)
ostream_t socket_get_ostream(socket_t sock)
{
    return sock | SOCKET_FLAG;
}

void socket_close(socket_t sock)
void socket_close(socket_t sock)
{
    if(sock != INVALID_SOCKET)
    {
#ifdef _WIN32
        closesocket(sock);

#else
        close(sock);
#endif
    }
}

static thread_return_t server_socket_run(void *data)
static thread_return_t server_socket_run(void *data)
{
    server_socket_t *server_socket = (server_socket_t *)data;
    server_socket_t *server_socket = (server_socket_t *)data;
    socket_t client_socket;
    socket_t client_socket;
#ifdef IPV6_SUPPORT
    struct sockaddr_in6 addr;
    struct sockaddr_in6 addr;
#else
    struct sockaddr_in addr;
    struct sockaddr_in addr;
#endif

#ifdef HAVE_INETNTOP
#ifdef IPV6_SUPPORT
    char straddr[INET6_ADDRSTRLEN];
#else
    char straddr[INET_ADDRSTRLEN];
#endif /* IPV6_SUPPORT */
#endif /* HAVE_INETNTOP */

    socklen_t addrlen = sizeof(addr);
    socklen_t addrlen = sizeof(addr);
    int r;
    MEMSET((char *)&addr, 0, sizeof(addr));

    while(server_socket->cont)
    while(server_socket->cont)
    {
        client_socket = accept(server_socket->socket, (struct sockaddr *)&addr, &addrlen);
        client_socket = accept(server_socket->socket, (struct sockaddr *)&addr, &addrlen);

        if(client_socket == INVALID_SOCKET)
        if(client_socket == INVALID_SOCKET)
        {
            server_socket->cont = 0;
            server_socket->cont = 0;
            return THREAD_RETURN_VALUE;
        }
        else
        {
#ifdef HAVE_INETNTOP

#ifdef IPV6_SUPPORT
            inet_ntop(AF_INET6, &addr.sin6_addr, straddr, sizeof(straddr));
            inet_ntop(AF_INET6, &addr.sin6_addr, straddr, sizeof(straddr));
#else
            inet_ntop(AF_INET, &addr.sin_addr, straddr, sizeof(straddr));
            inet_ntop(AF_INET, &addr.sin_addr, straddr, sizeof(straddr));
#endif

            r = server_socket->func(server_socket->data, client_socket,
            r = server_socket->func(server_socket->data, client_socket,
                                    straddr);
#else
            r = server_socket->func(server_socket->data, client_socket,
            r = server_socket->func(server_socket->data, client_socket,
                                    inet_ntoa(addr.sin_addr));
                                    inet_ntoa(addr.sin_addr));
#endif

            if(r != 0)
            {
                socket_close(client_socket);
                socket_close(client_socket);
            }
        }
    }

    return THREAD_RETURN_VALUE;
}

server_socket_t *
server_socket_t *
new_server_socket(int port, server_func_t func, void *data)
new_server_socket(int port, server_func_t func, void *data)
{
    server_socket_t *server_socket;
    server_socket_t *server_socket;
#ifdef IPV6_SUPPORT
    struct sockaddr_in6 addr;
    struct sockaddr_in6 addr;
#else
    struct sockaddr_in addr;
    struct sockaddr_in addr;
#endif

    socket_t sock;
    socket_t sock;

    return_val_if_fail(func != NULL, NULL);
    return_val_if_fail(func != NULL, NULL);

    if(socket_init() != OK)
    if(socket_init() != OK)
    {
        return NULL;
    }

#ifdef IPV6_SUPPORT
    sock = socket(AF_INET6, SOCK_STREAM, 0);

    if(sock == INVALID_SOCKET)
    {
        socket_cleanup();
        socket_cleanup();
        return NULL;
    }

    MEMSET(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons((uint16_t)port);
    addr.sin6_addr = in6addr_any;
    addr.sin6_addr = in6addr_any;
#else

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if(sock == INVALID_SOCKET)
    {
        socket_cleanup();
        socket_cleanup();
        return NULL;
    }

    MEMSET(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
#endif

    if(bind(sock, (const struct sockaddr *) &addr, sizeof(addr)) == SOCKET_ERROR)
    {
        socket_close(sock);
        socket_close(sock);
        socket_cleanup();
        socket_cleanup();
        return NULL;
    }

    if(listen(sock, SOMAXCONN) == SOCKET_ERROR)
    {
        socket_close(sock);
        socket_close(sock);
        socket_cleanup();
        socket_cleanup();
        return NULL;
    }

    server_socket = NEW(server_socket_t);
    server_socket = NEW(server_socket_t);

    if(server_socket == NULL)
    if(server_socket == NULL)
    {
        socket_close(sock);
        socket_close(sock);
        socket_cleanup();
        socket_cleanup();
        return NULL;
    }

    server_socket->socket = sock;
    server_socket->socket = sock;
    server_socket->func = func;
    server_socket->func = func;
    server_socket->data = data;
    server_socket->data = data;
    server_socket->cont = 1;
    server_socket->cont = 1;

    server_socket->thread = new_thread("server", server_socket_run, server_socket,
    server_socket->thread = new_thread("server", server_socket_run, server_socket,
                            0, FALSE);

    if(server_socket->thread == NULL)
    if(server_socket->thread == NULL)
    {
        FREE(server_socket);
        FREE(server_socket);
        socket_close(sock);
        socket_close(sock);
        socket_cleanup();
        socket_cleanup();
        return NULL;
    }

    return server_socket;
    return server_socket;
}

void delete_server_socket(server_socket_t *server_socket)
void delete_server_socket(server_socket_t *server_socket)
{
    return_if_fail(server_socket != NULL);
    return_if_fail(server_socket != NULL);

    server_socket->cont = 0;
    server_socket->cont = 0;

    if(server_socket->socket != INVALID_SOCKET)
    if(server_socket->socket != INVALID_SOCKET)
    {
        socket_close(server_socket->socket);
        socket_close(server_socket->socket);
    }

    if(server_socket->thread)
    if(server_socket->thread)
    {
        thread_join(server_socket->thread);
        thread_join(server_socket->thread);
        delete_thread(server_socket->thread);
        delete_thread(server_socket->thread);
    }

    FREE(server_socket);
    FREE(server_socket);

    // Should be called the same number of times as socket_init()
    // Should be called the same number of times as socket_init()
    socket_cleanup();
    socket_cleanup();
}

#endif // NETWORK_SUPPORT
