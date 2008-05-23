/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <assert.h>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "spwin32iocp.hpp"
#include "spiocpserver.hpp"

#include "spsession.hpp"
#include "spbuffer.hpp"
#include "spmsgdecoder.hpp"
#include "sprequest.hpp"
#include "sphandler.hpp"
#include "sputils.hpp"

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

class SP_EchoHandlerFactory : public SP_HandlerFactory {
public:
	SP_EchoHandlerFactory() {}
	virtual ~SP_EchoHandlerFactory() {}

	virtual SP_Handler * create() const {
		return new SP_EchoHandler();
	}
};

int main(void)
{
	int port = 3333;

	_CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);

	if( 0 != sp_initsock() ) assert( 0 );

	SP_IocpServer server( "", port, new SP_EchoHandlerFactory() );
	server.setTimeout( 0 );
	server.setMaxThreads( 4 );
	server.setMaxConnections( 10000 );
	server.runForever();

	_CrtDumpMemoryLeaks();

	return 0;
}
