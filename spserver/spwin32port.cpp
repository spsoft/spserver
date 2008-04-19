/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdarg.h>

#include "spwin32port.hpp"

/* Windows doesn't have writev() but does have WSASend */
int spwin32_writev(SOCKET sock, const struct iovec *vector, DWORD count)
{
	DWORD sent;
	WSASend(sock, (LPWSABUF)vector, count, &sent, 0, NULL, NULL);
	return sent;
}

int spwin32_inet_aton(const char *c, struct in_addr* addr)
{
	unsigned int r;
	if (strcmp(c, "255.255.255.255") == 0) {
		addr->s_addr = 0xFFFFFFFFu;
		return 1;
	}
	r = inet_addr(c);
	if (r == INADDR_NONE)
		return 0;
	addr->s_addr = r;
	return 1;
}

int spwin32_socketpair(int d, int type, int protocol, int socks[2])
{
    struct sockaddr_in addr;
    SOCKET listener;
    int e;
    int addrlen = sizeof(addr);
    DWORD flags = 0;
	
    if (socks == 0) {
		WSASetLastError(WSAEINVAL);
		return SOCKET_ERROR;
    }
	
    socks[0] = socks[1] = INVALID_SOCKET;
    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) 
        return SOCKET_ERROR;
	
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0x7f000001);
    addr.sin_port = 0;
	
    e = bind(listener, (const struct sockaddr*) &addr, sizeof(addr));
    if (e == SOCKET_ERROR) {
        e = WSAGetLastError();
		closesocket(listener);
        WSASetLastError(e);
        return SOCKET_ERROR;
    }
    e = getsockname(listener, (struct sockaddr*) &addr, &addrlen);
    if (e == SOCKET_ERROR) {
        e = WSAGetLastError();
		closesocket(listener);
        WSASetLastError(e);
        return SOCKET_ERROR;
    }
	
    do {
        if (listen(listener, 1) == SOCKET_ERROR) break;
        if ((socks[0] = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, flags)) == INVALID_SOCKET) break;
        if (connect(socks[0], (const struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) break;
        if ((socks[1] = accept(listener, NULL, NULL)) == INVALID_SOCKET) break;
        closesocket(listener);
        return 0;
    } while (0);
    e = WSAGetLastError();
    closesocket(listener);
    closesocket(socks[0]);
    closesocket(socks[1]);
    WSASetLastError(e);
    return SOCKET_ERROR;
}

int spwin32_gettimeofday(struct timeval* tv, void * ) 
{
	union {
		long ns100;
		FILETIME ft;
	} now;

	GetSystemTimeAsFileTime (&now.ft);
	tv->tv_usec = (long) ((now.ns100 / 10L ) % 1000000L);
	tv->tv_sec = (long) ((now.ns100 - 116444736000000000L) / 10000000L);
	return (0);
}

void spwin32_syslog (int priority, const char * format, ...)
{
	char logTemp[ 1024 ] = { 0 };

	va_list vaList;
	va_start( vaList, format );
	_vsnprintf( logTemp, sizeof( logTemp ), format, vaList );
	va_end ( vaList );

	printf( "%s\n", logTemp );
}

void spwin32_closelog (void)
{
}

void spwin32_openlog (const char *ident , int option , int facility)
{
}

int spwin32_setlogmask (int priority)
{
	return 0;
}

int spwin32_initsocket()
{
	WSADATA wsaData;
	
	int err = WSAStartup( MAKEWORD( 1, 1 ), &wsaData );
	if ( err != 0 ) {
		spwin32_syslog( LOG_EMERG, "Couldn't find a useable winsock.dll." );
		return -1;
	}

	return 0;
}
