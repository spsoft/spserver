/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "spdispatcher.hpp"
#include "spioutils.hpp"
#include "sptunnelimpl.hpp"

#ifdef OPENSSL
#include "spopenssl.hpp"
#else
#include "spmatrixssl.hpp"
#endif

int main( int argc, char * argv[] )
{
	int port = 8080, maxThreads = 10;
	const char * dstHost = "66.249.89.99";
	int dstPort = 80;

	extern char *optarg ;
	int c ;

	while( ( c = getopt ( argc, argv, "p:t:r:v" )) != EOF ) {
		switch ( c ) {
			case 'p' :
				port = atoi( optarg );
				break;
			case 't':
				maxThreads = atoi( optarg );
				break;
			case 'r':
			{
				dstHost = strdup( optarg );
				char * pos = strchr( dstHost, ':' );
				if( NULL != pos ) {
					dstPort = atoi( pos + 1 );
					*pos = '\0';
				}
				break;
			}
			case '?' :
			case 'v' :
				printf( "Usage: %s [-p <port>] [-t <threads>] [-r <backend>]\n", argv[0] );
				exit( 0 );
		}
	}

#ifdef LOG_PERROR
	openlog( "sptunnel", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER );
#else
	openlog( "sptunnel", LOG_CONS | LOG_PID, LOG_USER );
#endif

	syslog( LOG_NOTICE, "Backend server - %s:%d", dstHost, dstPort );

	int maxConnections = 100, reqQueueSize = 100;
	const char * refusedMsg = "System busy, try again later.";

	int listenFd = -1;
	if( 0 == SP_IOUtils::tcpListen( "", port, &listenFd ) ) {
		SP_Dispatcher dispatcher( new SP_DefaultCompletionHandler(), maxThreads );
		dispatcher.setTimeout( 60 );
		dispatcher.dispatch();

#ifdef	OPENSSL
		SP_OpensslChannelFactory * sslFactory = new SP_OpensslChannelFactory();
#else
		SP_MatrixsslChannelFactory * sslFactory = new SP_MatrixsslChannelFactory();
#endif
		sslFactory->init( "demo.crt", "demo.key" );

		for( ; ; ) {
			struct sockaddr_in addr;
			socklen_t socklen = sizeof( addr );
			int fd = accept( listenFd, (struct sockaddr*)&addr, &socklen );

			if( fd > 0 ) {
				if( dispatcher.getSessionCount() >= maxConnections
						|| dispatcher.getReqQueueLength() >= reqQueueSize ) {
					write( fd, refusedMsg, strlen( refusedMsg ) );
					close( fd );
				} else {
					SP_TunnelHandler * handler = new SP_TunnelHandler(
							&dispatcher, dstHost, dstPort );
					dispatcher.push( fd, handler, sslFactory->create() );

					// for non-ssl tunnel
					//dispatcher.push( fd, handler, new SP_DefaultIOChannel() );
				}
			} else {
				break;
			}
		}
	}

	closelog();

	return 0;
}

