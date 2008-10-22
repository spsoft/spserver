/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __sprequest_hpp__
#define __sprequest_hpp__

class SP_MsgDecoder;

class SP_Request {
public:
	SP_Request();
	~SP_Request();

	// default return SP_DefaultMsgDecoder
	SP_MsgDecoder * getMsgDecoder();

	// set a special SP_MsgDecoder
	void setMsgDecoder( SP_MsgDecoder * decoder );

	void setClientIP( const char * clientIP );
	const char * getClientIP();

	void setClientPort( int port );
	int getClientPort();

private:
	char mClientIP[ 32 ];
	int mPort;
	SP_MsgDecoder * mDecoder;
};

#endif

