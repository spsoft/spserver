/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <assert.h>

#include "spwin32port.hpp"

#include "spiocpdispatcher.hpp"
#include "sphandler.hpp"
#include "spresponse.hpp"
#include "sprequest.hpp"
#include "spbuffer.hpp"
#include "spmsgdecoder.hpp"
#include "spioutils.hpp"
#include "sputils.hpp"

#include "spgetopt.h"

#pragma comment(lib,"ws2_32")
#pragma comment(lib,"mswsock")

class SP_EchoHandler : public SP_Handler {
public:
	SP_EchoHandler(){}
	virtual ~SP_EchoHandler(){}

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response ) {
		request->setMsgDecoder( new SP_MultiLineMsgDecoder() );
		response->getReply()->getMsg()->append(
			"Welcome to line echo server, enter 'quit' to quit.\r\n" );

		return 0;
	}

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response ) {
		SP_MultiLineMsgDecoder * decoder = (SP_MultiLineMsgDecoder*)request->getMsgDecoder();
		SP_CircleQueue * queue = decoder->getQueue();

		int ret = 0;
		for( ; NULL != queue->top(); ) {
			char * line = (char*)queue->pop();

			if( 0 != strcasecmp( line, "quit" ) ) {
				response->getReply()->getMsg()->append( line );
				response->getReply()->getMsg()->append( "\r\n" );
			} else {
				response->getReply()->getMsg()->append( "Byebye\r\n" );
				ret = -1;
			}

			free( line );
		}

		return ret;
	}

	virtual void error( SP_Response * response ) {}

	virtual void timeout( SP_Response * response ) {}

	virtual void close() {}
};

class SP_EchoTimerHandler : public SP_TimerHandler {
public:
	SP_EchoTimerHandler(){
		mCount = 1;
	}

	virtual ~SP_EchoTimerHandler(){}

	// return -1 : terminate timer, 0 : continue
	virtual int handle( SP_Response * response, struct timeval * timeout ) {
		sp_syslog( LOG_NOTICE, "time = %li, call timer handler", time( NULL ) );

		if( ++mCount >= 10 ) {
			sp_syslog( LOG_NOTICE, "stop timer" );
			return -1;
		} else {
			sp_syslog( LOG_NOTICE, "set timer to %d seconds later", mCount );
			timeout->tv_sec = mCount;
			return 0;
		}
	}

private:
	int mCount;
};

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

	if( 0 != sp_initsock() ) assert( 0 );

	int maxConnections = 10000, reqQueueSize = 10000;
	const char * refusedMsg = "System busy, try again later.";

	int listenFd = -1;
	if( 0 == SP_IOUtils::tcpListen( "", port, &listenFd ) ) {
		SP_IocpDispatcher dispatcher( new SP_DefaultCompletionHandler(), maxThreads );
		dispatcher.dispatch();

		struct timeval timeout;
		memset( &timeout, 0, sizeof( timeout ) );
		timeout.tv_sec = 1;

		dispatcher.push( &timeout, new SP_EchoTimerHandler() );

		for( ; ; ) {
			struct sockaddr_in addr;
			socklen_t socklen = sizeof( addr );
			int fd = accept( listenFd, (struct sockaddr*)&addr, &socklen );

			if( INVALID_SOCKET != fd ) {
				if( dispatcher.getSessionCount() >= maxConnections
						|| dispatcher.getReqQueueLength() >= reqQueueSize ) {
					send( fd, refusedMsg, strlen( refusedMsg ), 0 );
					closesocket( fd );
				} else {
					dispatcher.push( fd, new SP_EchoHandler() );
				}
			} else {
				break;
			}
		}
	}

	return 0;
}

