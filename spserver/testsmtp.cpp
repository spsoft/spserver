/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

#include "spporting.hpp"

#include "spmsgdecoder.hpp"
#include "spbuffer.hpp"

#include "spserver.hpp"
#include "splfserver.hpp"
#include "sphandler.hpp"
#include "spresponse.hpp"
#include "sprequest.hpp"

class SP_SmtpHandler : public SP_Handler {
public:
	SP_SmtpHandler();
	virtual ~SP_SmtpHandler();

	virtual int start( SP_Request * request, SP_Response * response );

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response );

	virtual void error( SP_Response * response );

	virtual void timeout( SP_Response * response );

	virtual void close();

private:
	int mState;

	enum {
		eINIT, eHELO, eEHLO, eAUTH, eAUTHUSER, eAUTHPASS, eMAIL, eRCPT, eDATA, eMAILDATA, eNOOP, eQUIT
	};
};

SP_SmtpHandler :: SP_SmtpHandler()
{
	mState = eINIT;
}

SP_SmtpHandler :: ~SP_SmtpHandler()
{
}

int SP_SmtpHandler :: start( SP_Request * request, SP_Response * response )
{
	char buffer[ 128 ] = { 0 };
	snprintf( buffer, sizeof( buffer ),
		"220 SMTP Server Ready, Client %s\r\n", request->getClientIP() );

	request->setMsgDecoder( new SP_LineMsgDecoder() );
	response->getReply()->getMsg()->append( buffer );

	return 0;
}

int SP_SmtpHandler :: handle( SP_Request * request, SP_Response * response )
{
	int ret = 0;

	// request->getMsgDecoder will return the arg of request->setMsgDecoder
	const char * line = ((SP_LineMsgDecoder*)(request->getMsgDecoder()))->getMsg();

	SP_Buffer * outBuffer = response->getReply()->getMsg();

	if( mState == eDATA ) {
		request->setMsgDecoder( new SP_LineMsgDecoder() );

		outBuffer->append( "250 Requested mail action okay, completed.\r\n" );
		mState = eMAILDATA;
	} else if( mState == eAUTH ) {
		outBuffer->append( "354 UGFzc3dvcmQ6\r\n" );
		mState = eAUTHUSER;
	} else if( mState == eAUTHUSER ) {
		outBuffer->append( "235 OK Authenticated\r\n" );
		mState = eAUTHPASS;
	} else if( 0 == strncasecmp( line, "HELO", 4 ) ) {
		outBuffer->append( "250 ok.\r\n" );
		mState = eHELO;
	} else if( 0 == strncasecmp( line, "EHLO", 4 ) ) {
		outBuffer->append( "250-iunknown.com, helo.\r\n"
				"250-EXPN\r\n"
				"250 XTMD\r\n" );
		mState = eEHLO;
	} else if( 0 == strncasecmp( line, "AUTH", 4 ) ) {
		outBuffer->append( "354 VXNlcm5hbWU6\r\n" );
		mState = eAUTH;
	} else if( 0 == strncasecmp( line, "MAIL", 4 ) ) {
		outBuffer->append( "250 sender ok.\r\n" );
		mState = eMAIL;
	} else if( 0 == strncasecmp( line, "RCPT", 4 ) ) {
		outBuffer->append( "250 recipient ok.\r\n" );
		mState = eRCPT;
	} else if( 0 == strncasecmp( line, "DATA", 4 ) ) {
		request->setMsgDecoder( new SP_DotTermMsgDecoder() );

		outBuffer->append( "354 Start mail input; end with <CRLF>.<CRLF>\r\n" );
		mState = eDATA;
	} else if( 0 == strncasecmp( line, "NOOP", 4 ) ) {
		outBuffer->append( "250 ok.\r\n" );
		mState = eNOOP;
	} else if( 0 == strncasecmp( line, "QUIT", 4 ) ) {
		outBuffer->append( "221 Closing connection. Good bye.\r\n" );
		ret = -1;
		mState = eQUIT;
	} else {
		outBuffer->append( "500 Syntax error, command unrecognized.\r\n" );
	}

	return ret;
}

void SP_SmtpHandler :: error( SP_Response * )
{
}

void SP_SmtpHandler :: timeout( SP_Response * )
{
}

void SP_SmtpHandler :: close()
{
}

//---------------------------------------------------------

class SP_SmtpHandlerFactory : public SP_HandlerFactory {
public:
	SP_SmtpHandlerFactory();
	virtual ~SP_SmtpHandlerFactory();

	virtual SP_Handler * create() const;

	//use default SP_CompletionHandler is enough, not need to implement
	//virtual SP_CompletionHandler * createCompletionHandler() const;
};

SP_SmtpHandlerFactory :: SP_SmtpHandlerFactory()
{
}

SP_SmtpHandlerFactory :: ~SP_SmtpHandlerFactory()
{
}

SP_Handler * SP_SmtpHandlerFactory :: create() const
{
	return new SP_SmtpHandler();
}

//---------------------------------------------------------

int main( int argc, char * argv[] )
{
	int port = 1025, maxThreads = 10;
	const char * serverType = "hahs";

#ifndef WIN32
	extern char *optarg ;
	int c ;

	while( ( c = getopt ( argc, argv, "p:t:s:v" )) != EOF ) {
		switch ( c ) {
			case 'p' :
				port = atoi( optarg );
				break;
			case 't':
				maxThreads = atoi( optarg );
				break;
			case 's':
				serverType = optarg;
				break;
			case '?' :
			case 'v' :
				printf( "Usage: %s [-p <port>] [-t <threads>] [-s <hahs|lf>]\n", argv[0] );
				exit( 0 );
		}
	}
#endif

#ifdef LOG_PERROR
	sp_openlog( "testsmtp", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER );
#else
	sp_openlog( "testsmtp", LOG_CONS | LOG_PID, LOG_USER );
#endif

	assert( 0 == sp_initsock() );

	if( 0 == strcasecmp( serverType, "hahs" ) ) {
		SP_Server server( "", port, new SP_SmtpHandlerFactory() );

		server.setMaxConnections( 2048 );
		server.setTimeout( 600 );
		server.setMaxThreads( maxThreads );
		server.setReqQueueSize( 100, "Sorry, server is busy now!\n" );

		server.runForever();
	} else {
		SP_LFServer server( "", port, new SP_SmtpHandlerFactory() );

		server.setMaxConnections( 2048 );
		server.setTimeout( 600 );
		server.setMaxThreads( maxThreads );
		server.setReqQueueSize( 100, "Sorry, server is busy now!\n" );

		server.runForever();
	}

	sp_closelog();

	return 0;
}

