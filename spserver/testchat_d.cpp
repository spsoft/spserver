/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <assert.h>

#include "spthread.hpp"

#include "spmsgdecoder.hpp"
#include "spbuffer.hpp"

#include "spdispatcher.hpp"
#include "sphandler.hpp"
#include "spresponse.hpp"
#include "sprequest.hpp"
#include "spioutils.hpp"

#ifdef WIN32
#include "spgetopt.h"
#endif

class SP_OnlineSidList {
public:
	SP_OnlineSidList();
	~SP_OnlineSidList();

	void copy( SP_SidList * outList, SP_Sid_t * ignoreSid = NULL );
	void remove( SP_Sid_t sid );
	void add( SP_Sid_t sid );
	int getCount();

private:
	SP_SidList mList;
	sp_thread_mutex_t mMutex;
};

SP_OnlineSidList :: SP_OnlineSidList()
{
	sp_thread_mutex_init( &mMutex, NULL );
}

SP_OnlineSidList :: ~SP_OnlineSidList()
{
	sp_thread_mutex_destroy( &mMutex );
}

void SP_OnlineSidList :: copy( SP_SidList * outList, SP_Sid_t * ignoreSid )
{
	sp_thread_mutex_lock( &mMutex );

	for( int i = 0; i < mList.getCount(); i++ ) {
		if( NULL != ignoreSid ) {
			SP_Sid_t theSid = mList.get( i );
			if( theSid.mKey == ignoreSid->mKey && theSid.mSeq == ignoreSid->mSeq ) {
				continue;
			}
		}

		outList->add( mList.get( i ) );
	}

	sp_thread_mutex_unlock( &mMutex );
}

void SP_OnlineSidList :: remove( SP_Sid_t sid )
{
	sp_thread_mutex_lock( &mMutex );

	for( int i = 0; i < mList.getCount(); i++ ) {
		SP_Sid_t theSid = mList.get( i );
		if( theSid.mKey == sid.mKey && theSid.mSeq == sid.mSeq ) {
			mList.take( i );
			break;
		}
	}

	sp_thread_mutex_unlock( &mMutex );
}

void SP_OnlineSidList :: add( SP_Sid_t sid )
{
	sp_thread_mutex_lock( &mMutex );

	mList.add( sid );

	sp_thread_mutex_unlock( &mMutex );
}

int SP_OnlineSidList :: getCount()
{
	int count = 0;

	sp_thread_mutex_lock( &mMutex );

	count = mList.getCount();

	sp_thread_mutex_unlock( &mMutex );

	return count;
}

//---------------------------------------------------------

class SP_ChatHandler : public SP_Handler {
public:
	SP_ChatHandler( SP_OnlineSidList * onlineSidList, int chatID );
	virtual ~SP_ChatHandler();

	virtual int start( SP_Request * request, SP_Response * response );

	// return -1 : terminate session, 0 : continue
	virtual int handle( SP_Request * request, SP_Response * response );

	virtual void error( SP_Response * response );

	virtual void timeout( SP_Response * response );

	virtual void close();

private:
	SP_Sid_t mSid;
	int mChatID;

	SP_OnlineSidList * mOnlineSidList;

	static int mMsgSeq;

	void broadcast( SP_Response * response, const char * buffer, SP_Sid_t * ignoreSid = 0 );
};

int SP_ChatHandler :: mMsgSeq = 0;

SP_ChatHandler :: SP_ChatHandler( SP_OnlineSidList * onlineSidList, int chatID )
{
	memset( &mSid, 0, sizeof( mSid ) );
	mChatID = chatID;

	mOnlineSidList = onlineSidList;
}

SP_ChatHandler :: ~SP_ChatHandler()
{
}

void SP_ChatHandler :: broadcast( SP_Response * response, const char * buffer, SP_Sid_t * ignoreSid )
{
	if( mOnlineSidList->getCount() > 0 ) {
		SP_Message * msg = new SP_Message();
		mOnlineSidList->copy( msg->getToList(), ignoreSid );
		msg->setCompletionKey( ++mMsgSeq );

		msg->getMsg()->append( buffer );
		response->addMessage( msg );
	}
}

int SP_ChatHandler :: start( SP_Request * request, SP_Response * response )
{
	request->setMsgDecoder( new SP_LineMsgDecoder() );

	mSid = response->getFromSid();

	char buffer[ 128 ] = { 0 };
	snprintf( buffer, sizeof( buffer ),
		"Welcome %d to chat server, enter 'quit' to quit.\r\n", mChatID );
	response->getReply()->getMsg()->append( buffer );
	response->getReply()->setCompletionKey( ++mMsgSeq );

	mOnlineSidList->add( mSid );

	return 0;
}

int SP_ChatHandler :: handle( SP_Request * request, SP_Response * response )
{
	SP_LineMsgDecoder * decoder = (SP_LineMsgDecoder*)request->getMsgDecoder();

	char buffer[ 256 ] = { 0 };

	if( 0 != strcasecmp( (char*)decoder->getMsg(), "quit" ) ) {
		snprintf( buffer, sizeof( buffer ), "%d say: %s\r\n", mChatID, (char*)decoder->getMsg() );
		broadcast( response, buffer );

		return 0;
	} else {
		snprintf( buffer, sizeof( buffer ), "SYS : %d normal offline\r\n", mChatID );
		broadcast( response, buffer, &mSid );

		response->getReply()->getMsg()->append( "SYS : Byebye\r\n" );
		response->getReply()->setCompletionKey( ++mMsgSeq );

		return -1;
	}
}

void SP_ChatHandler :: error( SP_Response * response )
{
	char buffer[ 64 ] = { 0 };
	snprintf( buffer, sizeof( buffer ), "SYS : %d error offline\r\n", mChatID );

	broadcast( response, buffer, &mSid );
}

void SP_ChatHandler :: timeout( SP_Response * response )
{
	char buffer[ 64 ] = { 0 };
	snprintf( buffer, sizeof( buffer ), "SYS : %d timeout offline\r\n", mChatID );

	broadcast( response, buffer, &mSid );
}

void SP_ChatHandler :: close()
{
	mOnlineSidList->remove( mSid );
}

//---------------------------------------------------------

class SP_ChatCompletionHandler : public SP_CompletionHandler {
public:
	SP_ChatCompletionHandler();
	~SP_ChatCompletionHandler();
	virtual void completionMessage( SP_Message * msg );
};

SP_ChatCompletionHandler :: SP_ChatCompletionHandler()
{
}

SP_ChatCompletionHandler :: ~SP_ChatCompletionHandler()
{
}

void SP_ChatCompletionHandler :: completionMessage( SP_Message * msg )
{
#if 0
	printf( "message completed { completion key : %d }\n", msg->getCompletionKey() );

	printf( "\tsuccess {" );
	for( int i = 0; i < msg->getSuccess()->getCount(); i++ ) {
		printf( " %d", msg->getSuccess()->get( i ).mKey );
	}
	printf( "}\n" );

	printf( "\tfailure {" );
	for( int i = 0; i < msg->getFailure()->getCount(); i++ ) {
		printf( " %d", msg->getFailure()->get( i ).mKey );
	}
	printf( "}\n" );
#endif

	delete msg;
}

//---------------------------------------------------------

int main( int argc, char * argv[] )
{
	int port = 5555, maxThreads = 10;

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
	sp_openlog( "testchat_d", LOG_CONS | LOG_PID | LOG_PERROR, LOG_USER );
#else
	sp_openlog( "testchat_d", LOG_CONS | LOG_PID, LOG_USER );
#endif

	assert( 0 == sp_initsock() );

	SP_OnlineSidList onlineSidList;
	int chatID = 0;

	int maxConnections = 100, reqQueueSize = 10;
	const char * refusedMsg = "System busy, try again later.";

	int listenFd = -1;
	if( 0 == SP_IOUtils::tcpListen( "", port, &listenFd ) ) {
		SP_Dispatcher dispatcher( new SP_ChatCompletionHandler(), maxThreads );
		dispatcher.dispatch();

		for( ; ; ) {
			struct sockaddr_in addr;
			socklen_t socklen = sizeof( addr );
			int fd = accept( listenFd, (struct sockaddr*)&addr, &socklen );

			if( fd > 0 ) {
				if( dispatcher.getSessionCount() >= maxConnections
						|| dispatcher.getReqQueueLength() >= reqQueueSize ) {
					send( fd, refusedMsg, strlen( refusedMsg ), 0 );
					sp_close( fd );
				} else {

					char buffer[ 256 ] = { 0 };
					snprintf( buffer, sizeof( buffer ), "SYS : %d online\r\n", ++chatID );

					SP_Message * msg = new SP_Message();
					onlineSidList.copy( msg->getToList(), NULL );
					msg->getMsg()->append( buffer );

					SP_Sid_t sid = { SP_Sid_t::ePushKey, SP_Sid_t::ePushSeq };
					SP_Response * response = new SP_Response( sid );
					response->addMessage( msg );

					dispatcher.push( response );

					dispatcher.push( fd, new SP_ChatHandler( &onlineSidList, chatID ) );
				}
			} else {
				break;
			}
		}
	}

	sp_closelog();

	return 0;
}

