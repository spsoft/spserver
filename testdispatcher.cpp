/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "spdispatcher.hpp"
#include "sphandler.hpp"
#include "spresponse.hpp"
#include "sprequest.hpp"
#include "spbuffer.hpp"
#include "spmsgdecoder.hpp"

class SP_EchoHandler : public SP_Handler {
public:
	SP_EchoHandler(){}
	virtual ~SP_EchoHandler(){}

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response ) {
		request->setMsgDecoder( new SP_LineMsgDecoder() );
		response->getReply()->getMsg()->append(
			"Welcome to line echo dispatcher, enter 'quit' to quit.\r\n" );

		return 0;
	}

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response ) {
		SP_LineMsgDecoder * decoder = (SP_LineMsgDecoder*)request->getMsgDecoder();

		if( 0 != strcasecmp( (char*)decoder->getMsg(), "quit" ) ) {
			response->getReply()->getMsg()->append( (char*)decoder->getMsg() );
			response->getReply()->getMsg()->append( "\r\n" );
			return 0;
		} else {
			response->getReply()->getMsg()->append( "Byebye\r\n" );
			return -1;
		}
	}

	virtual void error( SP_Response * response ) {}

	virtual void timeout( SP_Response * response ) {}

	virtual void close() {}
};

static int sp_listen( int port, int * fd )
{
	int ret = 0;

	int listenFd = socket( AF_INET, SOCK_STREAM, 0 );
	if( listenFd < 0 ) {
		syslog( LOG_WARNING, "listen failed" );
		ret = -1;
	}

	if( 0 == ret ) {
		int flags = 1;
		if( setsockopt( listenFd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof( flags ) ) < 0 ) {
			syslog( LOG_WARNING, "setsockopt failed" );
			ret = -1;
		}
	}

	struct sockaddr_in addr;
	memset( &addr, 0, sizeof( addr ) );
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons( port );

	if( 0 == ret ) {
		if( bind( listenFd, (struct sockaddr*)&addr, sizeof( addr ) ) < 0 ) {
			syslog( LOG_WARNING, "bind failed" );
			ret = -1;
		}
	}

	if( 0 == ret ) {
		if( listen( listenFd, 5 ) < 0 ) {
			syslog( LOG_WARNING, "listen failed" );
			ret = -1;
		}
	}

	if( 0 != ret && listenFd >= 0 ) close( listenFd );

	if( 0 == ret ) {
		* fd = listenFd;
		syslog( LOG_NOTICE, "Listen on port [%d]", port );
	}

	return ret;
}

int main( int argc, char * argv[] )
{
	int port = 3333, maxThreads = 10;

	extern char *optarg ;
	int c ;

	while( ( c = getopt ( argc, argv, "p:t:v" )) != EOF ) {
		switch ( c ) {
			case 'p' :
				port = atoi( optarg );
				break;
			case 't':
				maxThreads = atoi( optarg );
				break;
			case '?' :
			case 'v' :
				printf( "Usage: %s [-p <port>] [-t <threads>]\n", argv[0] );
				exit( 0 );
		}
	}

#ifdef LOG_PERROR
	openlog( "testdispatcher", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER );
#else
	openlog( "testdispatcher", LOG_CONS | LOG_PID, LOG_USER );
#endif

	int maxConnections = 10, reqQueueSize = 10;
	const char * refusedMsg = "System busy, try again later.";

	int listenFd = -1;
	if( 0 == sp_listen( port, &listenFd ) ) {
		SP_Dispatcher dispatcher( new SP_DefaultCompletionHandler(), maxThreads );
		dispatcher.dispatch();

		for( ; ; ) {
			struct sockaddr_in addr;
			socklen_t socklen = sizeof( addr );
			int fd = accept( listenFd, (struct sockaddr*)&addr, &socklen );

			if( fd > 0 ) {
				if( dispatcher.getSessionCount() >= maxConnections
						|| dispatcher.getReqQueueLength() >= reqQueueSize ) {
					write( fd, refusedMsg, strlen( refusedMsg ) );
				} else {
					dispatcher.push( fd, new SP_EchoHandler() );
				}
			} else {
				break;
			}
		}
	}

	closelog();

	return 0;
}

