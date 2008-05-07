/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spwin32port_hpp__
#define __spwin32port_hpp__

#include <winsock2.h>
#include <windows.h>
#include <io.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned short uint16_t;
typedef int socklen_t;

#define snprintf _snprintf
#define strcasecmp stricmp
#define strncasecmp strnicmp
#define pause()	Sleep((32767L << 16) + 32767)
#define sleep(x) Sleep(x*1000)

#define sp_close        closesocket
#define sp_writev       spwin32_writev
#define sp_inet_aton    spwin32_inet_aton
#define sp_socketpair   spwin32_socketpair
#define sp_initsock     spwin32_initsocket
#define sp_gettimeofday spwin32_gettimeofday

#define sp_syslog       spwin32_syslog
#define sp_openlog      spwin32_openlog
#define sp_closelog     spwin32_closelog
#define sp_setlogmask   spwin32_setlogmask

/* Windows writev() support */
struct iovec
{
	u_long	iov_len;
	char	*iov_base;
};

extern int spwin32_writev(SOCKET sock, const struct iovec *vector, DWORD count);

extern int spwin32_inet_aton(const char *c, struct in_addr* addr);

extern int spwin32_socketpair(int d, int type, int protocol, int sv[2]);

/* @return 0 OK, -1 Fail */
extern int spwin32_initsocket();

extern int spwin32_gettimeofday(struct timeval* tv, void * );

/* Windows syslog() support */

#define	LOG_EMERG	0
#define	LOG_ALERT	1
#define	LOG_CRIT	2
#define	LOG_ERR		3
#define	LOG_WARNING	4
#define	LOG_NOTICE	5
#define	LOG_INFO	6
#define	LOG_DEBUG	7

/*
 * Option flags for openlog.
 *
 * LOG_ODELAY no longer does anything.
 * LOG_NDELAY is the inverse of what it used to be.
 */
#define LOG_PID         0x01    /* log the pid with each message */
#define LOG_CONS        0x02    /* log on the console if errors in sending */
#define LOG_ODELAY      0x04    /* delay open until first syslog() (default) */
#define LOG_NDELAY      0x08    /* don't delay open */
#define LOG_NOWAIT      0x10    /* don't wait for console forks: DEPRECATED */
#define LOG_PERROR      0x20    /* log to stderr as well */

#define	LOG_USER	(1<<3)

extern void spwin32_syslog (int priority, const char * format, ...);
extern void spwin32_closelog (void);
extern void spwin32_openlog (const char *ident , int option , int facility);
extern int spwin32_setlogmask (int priority);

#ifdef __cplusplus
}
#endif

#endif
