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
#include "sphandler.hpp"
#include "spresponse.hpp"
#include "sprequest.hpp"

class SP_EchoHandler : public SP_Handler {
public:
	SP_EchoHandler(){}
	virtual ~SP_EchoHandler(){}

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response ) {
		request->setMsgDecoder( new SP_LineMsgDecoder() );
		response->getReply()->getMsg()->append(
			"Welcome to line echo server, enter 'quit' to quit.\r\n" );

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

class SP_EchoHandlerFactory : public SP_HandlerFactory {
public:
	SP_EchoHandlerFactory() {}
	virtual ~SP_EchoHandlerFactory() {}

	virtual SP_Handler * create() const {
		return new SP_EchoHandler();
	}
};

//---------------------------------------------------------

int main( int argc, char * argv[] )
{
	int port = 3333;

	assert( 0 == sp_initsock() );

	SP_Server server( "", port, new SP_EchoHandlerFactory() );
	server.runForever();

	return 0;
}

