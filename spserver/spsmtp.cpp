/*
 * Copyright 2009 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "spsmtp.hpp"

#include "spbuffer.hpp"
#include "sphandler.hpp"
#include "sprequest.hpp"
#include "spresponse.hpp"
#include "spmsgdecoder.hpp"
#include "sputils.hpp"

SP_SmtpHandler :: ~SP_SmtpHandler()
{
}

void SP_SmtpHandler :: error()
{
}

void SP_SmtpHandler :: timeout()
{
}

int SP_SmtpHandler :: welcome( SP_Buffer * reply )
{
	reply->append( "220 SMTP service ready\n" );

	return 0;
}

int SP_SmtpHandler :: helo( const char * args, SP_Buffer * reply )
{
	reply->append( "250 OK\n" );

	return 0;
}

int SP_SmtpHandler :: noop( const char * args, SP_Buffer * reply )
{
	reply->append( "250 OK\n" );

	return 0;
}

//---------------------------------------------------------

SP_SmtpHandlerFactory :: ~SP_SmtpHandlerFactory()
{
}

//---------------------------------------------------------

class SP_SmtpSession {
public:
	SP_SmtpSession( SP_SmtpHandlerFactory * handlerFactory );
	~SP_SmtpSession();

	void setSeenHelo( int seenHelo );
	int  getSeenHelo();

	void setSeenSender( int seenSender );
	int  getSeenSender();

	void addRcpt();
	int  getRcptCount();

	void setDataMode( int mode );
	int  getDataMode();

	int  getSeenData();

	void reset();

	SP_SmtpHandler * getHandler();

private:
	int mSeenHelo;
	int mSeenSender;
	int mRcptCount;
	int mSeenData;

	int mDataMode;

	SP_SmtpHandler * mHandler;

	SP_SmtpHandlerFactory * mHandlerFactory;
};

SP_SmtpSession :: SP_SmtpSession( SP_SmtpHandlerFactory * handlerFactory )
{
	mHandler = NULL;
	mHandlerFactory = handlerFactory;

	mSeenHelo = 0;

	reset();
}

SP_SmtpSession :: ~SP_SmtpSession()
{
	if( NULL != mHandler ) delete mHandler;
}

void SP_SmtpSession :: reset()
{
	if( NULL != mHandler ) delete mHandler;
	mHandler = NULL;

	mSeenSender = 0;
	mRcptCount = 0;
	mSeenData = 0;

	mDataMode = 0;
}

void SP_SmtpSession :: setSeenHelo( int seenHelo )
{
	mSeenHelo = seenHelo;
}

int  SP_SmtpSession :: getSeenHelo()
{
	return mSeenHelo;
}

void SP_SmtpSession :: setSeenSender( int seenSender )
{
	mSeenSender = seenSender;
}

int  SP_SmtpSession :: getSeenSender()
{
	return mSeenSender;
}

void SP_SmtpSession :: addRcpt()
{
	mRcptCount++;
}

int  SP_SmtpSession :: getRcptCount()
{
	return mRcptCount;
}

void SP_SmtpSession :: setDataMode( int mode )
{
	mDataMode = mode;

	if( 1 == mode ) mSeenData = 1;
}

int  SP_SmtpSession :: getDataMode()
{
	return mDataMode;
}

int  SP_SmtpSession :: getSeenData()
{
	return mSeenData;
}

SP_SmtpHandler * SP_SmtpSession :: getHandler()
{
	if( NULL == mHandler ) mHandler = mHandlerFactory->create();

	return mHandler;
}

//---------------------------------------------------------

class SP_SmtpHandlerAdapter : public SP_Handler {
public:
	SP_SmtpHandlerAdapter( SP_SmtpHandlerFactory * handlerFactory );

	virtual ~SP_SmtpHandlerAdapter();

	// return -1 : terminate session, 0 : continue
	virtual int start( SP_Request * request, SP_Response * response );

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response );

	virtual void error( SP_Response * response );

	virtual void timeout( SP_Response * response );

	virtual void close();

private:
	SP_SmtpSession * mSession;
};

SP_SmtpHandlerAdapter :: SP_SmtpHandlerAdapter( SP_SmtpHandlerFactory * handlerFactory )
{
	mSession = new SP_SmtpSession( handlerFactory );
}

SP_SmtpHandlerAdapter :: ~SP_SmtpHandlerAdapter()
{
	delete mSession;
}

int SP_SmtpHandlerAdapter :: start( SP_Request * request, SP_Response * response )
{
	int ret = mSession->getHandler()->welcome( response->getReply()->getMsg() );

	request->setMsgDecoder( new SP_LineMsgDecoder() );

	return ret;
}

int SP_SmtpHandlerAdapter :: handle( SP_Request * request, SP_Response * response )
{
	int ret = 0;

	SP_Buffer * reply = response->getReply()->getMsg();

	if( mSession->getDataMode() ) {
		SP_DotTermMsgDecoder * decoder = (SP_DotTermMsgDecoder*)request->getMsgDecoder();
		ret = mSession->getHandler()->data( decoder->getMsg(), reply );
		mSession->setDataMode( 0 );
		request->setMsgDecoder( new SP_LineMsgDecoder() );
	} else {
		const char * line = ((SP_LineMsgDecoder*)(request->getMsgDecoder()))->getMsg();

		char cmd[ 128 ] = { 0 };
		const char * args = NULL;

		sp_strtok( line, 0, cmd, sizeof( cmd ), ' ', &args );

		if( 0 == strcasecmp( cmd, "HELO" ) ) {
			if( NULL != args ) {
				if( 0 == mSession->getSeenHelo() ) {
					ret = mSession->getHandler()->helo( args, reply );
					if( 0 == ret ) mSession->setSeenHelo( 1 );
				} else {
					reply->append( "503 Duplicate HELO\r\n" );
				}
			} else {
				reply->append( "501 Syntax: HELO <hostname>\r\n" );
			}

		} else if( 0 == strcasecmp( cmd, "MAIL" ) ) {
			if( 0 == mSession->getSeenHelo() ) {
				reply->append( "503 Error: send HELO first\r\n" );
			} else if( mSession->getSeenSender() ) {
				reply->append( "503 Sender already specified.\r\n" );
			} else {
				if( NULL != args ) {
					if( 0 == strncasecmp( args, "FROM:", 5 ) ) args += 5;
					for( ; isspace( *args ); ) args++;
					ret = mSession->getHandler()->from( args, reply );
					if( 0 == ret ) mSession->setSeenSender( 1 );
				} else {
					reply->append( "501 Syntax: MAIL FROM:<address>\r\n" );
				}
			}

		} else if( 0 == strcasecmp( cmd, "RCPT" ) ) {
			if( 0 == mSession->getSeenHelo() ) {
				reply->append( "503 Error: send HELO first\r\n" );
			} else if( 0 == mSession->getSeenSender() ) {
				reply->append( "503 Error: need MAIL command\r\n" );
			} else if( mSession->getSeenData() ) {
				reply->append( "503 Bad sequence of commands\r\n" );
			} else {
				if( NULL != args ) {
					if( 0 == strncasecmp( args, "TO:", 3 ) ) args += 3;
					for( ; isspace( *args ); ) args++;
					ret = mSession->getHandler()->rcpt( args, reply );
					if( 0 == ret ) mSession->addRcpt();
				} else {
					reply->append( "501 Syntax: RCPT TO:<address>\r\n" );
				}
			}

		} else if( 0 == strcasecmp( cmd, "DATA" ) ) {
			if( 0 == mSession->getSeenHelo() ) {
				reply->append( "503 Error: send HELO first\r\n" );
			} else if( 0 == mSession->getSeenSender() ) {
				reply->append( "503 Error: need MAIL command\r\n" );
			} else if( mSession->getRcptCount() <= 0 ) {
				reply->append( "503 Error: need RCPT command\r\n" );
			} else {
				request->setMsgDecoder( new SP_DotTermMsgDecoder() );
				reply->append( "354 Start mail input; end with <CRLF>.<CRLF>\r\n" );
				mSession->setDataMode( 1 );
			}

		} else if( 0 == strcasecmp( cmd, "RSET" ) ) {
			mSession->reset();
			reply->append( "250 OK\r\n" );

		} else if( 0 == strcasecmp( cmd, "NOOP" ) ) {
			ret = mSession->getHandler()->noop( args, reply );

		} else if( 0 == strcasecmp( cmd, "QUIT" ) ) {
			reply->append( "221 Closing connection. Good bye.\r\n" );
			ret = -1;

		} else {
			reply->append( "500 Syntax error, command unrecognized.\r\n" );
		}
	}

	return ret;
}

void SP_SmtpHandlerAdapter :: error( SP_Response * response )
{
	mSession->getHandler()->error();
}

void SP_SmtpHandlerAdapter :: timeout( SP_Response * response )
{
	mSession->getHandler()->timeout();
}

void SP_SmtpHandlerAdapter :: close()
{
}

//---------------------------------------------------------

SP_SmtpHandlerAdapterFactory :: SP_SmtpHandlerAdapterFactory( SP_SmtpHandlerFactory * factory )
{
	mFactory = factory;
}

SP_SmtpHandlerAdapterFactory :: ~SP_SmtpHandlerAdapterFactory()
{
	delete mFactory;
}

SP_Handler * SP_SmtpHandlerAdapterFactory :: create() const
{
	return new SP_SmtpHandlerAdapter( mFactory );
}

