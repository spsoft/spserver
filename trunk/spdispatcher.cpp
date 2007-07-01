/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "spdispatcher.hpp"

#include "speventcb.hpp"
#include "sphandler.hpp"
#include "spsession.hpp"
#include "spexecutor.hpp"

#include "config.h"
#include "event_msgqueue.h"

SP_Dispatcher :: SP_Dispatcher( SP_CompletionHandler * completionHandler, int maxThreads )
{
	/* Don't die with SIGPIPE on remote read shutdown. That's dumb. */
	signal( SIGPIPE, SIG_IGN );

	mIsShutdown = 0;
	mIsRunning = 0;

	mEventArg = (SP_EventArg_t *)malloc( sizeof(SP_EventArg_t) );
	memset( mEventArg, 0, sizeof( SP_EventArg_t ) );

	mEventArg->mEventBase = (struct event_base*)event_init();
	mEventArg->mResponseQueue = msgqueue_new( mEventArg->mEventBase, 0,
			SP_EventCallback::onResponse, mEventArg );

	mEventArg->mTimeout = 600;
	mEventArg->mSessionManager = new SP_SessionManager();

	mEventArg->mCompletionHandler = completionHandler;

	maxThreads = maxThreads > 0 ? maxThreads : 64;
	mEventArg->mExecutor = new SP_Executor( maxThreads, "work" );
	mEventArg->mCompletionExecutor = new SP_Executor( 1, "act" );

	mPushQueue = msgqueue_new( mEventArg->mEventBase, 0, onPush, mEventArg );
}

SP_Dispatcher :: ~SP_Dispatcher()
{
	if( 0 == mIsRunning ) sleep( 1 );

	shutdown();

	for( ; mIsRunning; ) sleep( 1 );

	delete mEventArg->mCompletionExecutor;
	delete mEventArg->mExecutor;

	//msgqueue_destroy( (struct event_msgqueue*)mPushQueue );
	//msgqueue_destroy( (struct event_msgqueue*)mEventArg->mResponseQueue );
	//event_base_free( mEventArg->mEventBase );

	delete mEventArg->mSessionManager;
	delete mEventArg->mCompletionHandler;

	free( mEventArg );
	mEventArg = NULL;
}

void SP_Dispatcher :: setTimeout( int timeout )
{
	mEventArg->mTimeout = timeout > 0 ? timeout : mEventArg->mTimeout;
}

void SP_Dispatcher :: shutdown()
{
	mIsShutdown = 1;
}

int SP_Dispatcher :: isRunning()
{
	return mIsRunning;
}

int SP_Dispatcher :: getSessionCount()
{
	return mEventArg->mSessionManager->getCount();
}

int SP_Dispatcher :: getReqQueueLength()
{
	return mEventArg->mExecutor->getQueueLength();
}

int SP_Dispatcher :: dispatch()
{
	int ret = -1;

	pthread_attr_t attr;
	pthread_attr_init( &attr );
	assert( pthread_attr_setstacksize( &attr, 1024 * 1024 ) == 0 );
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

	pthread_t thread = 0;
	ret = pthread_create( &thread, &attr, reinterpret_cast<void*(*)(void*)>(eventLoop), this );
	pthread_attr_destroy( &attr );
	if( 0 == ret ) {
		syslog( LOG_NOTICE, "Thread #%ld has been created for dispatcher", thread );
	} else {
		mIsRunning = 0;
		syslog( LOG_WARNING, "Unable to create a thread for dispatcher, %s",
			strerror( errno ) ) ;
	}

	return ret;
}

void * SP_Dispatcher :: eventLoop( void * arg )
{
	SP_Dispatcher * dispatcher = (SP_Dispatcher*)arg;

	dispatcher->mIsRunning = 1;

	dispatcher->start();

	dispatcher->mIsRunning = 0;

	return NULL;
}

int SP_Dispatcher :: start()
{
	/* Start the event loop. */
	while( 0 == mIsShutdown ) {
		event_base_loop( mEventArg->mEventBase, EVLOOP_ONCE );
	}

	syslog( LOG_NOTICE, "Dispatcher is shutdown." );

	return 0;
}

typedef struct tagSP_PushArg {
	int mFd;
	SP_Handler * mHandler;
	int mNeedStart;
} SP_PushArg_t;

void SP_Dispatcher :: onPush( void * queueData, void * arg )
{
	SP_PushArg_t * pushArg = (SP_PushArg_t*)queueData;
	SP_EventArg_t * eventArg = (SP_EventArg_t*)arg;

	SP_Sid_t sid;
	sid.mKey = pushArg->mFd;
	eventArg->mSessionManager->get( sid.mKey, &sid.mSeq );

	SP_Session * session = new SP_Session( sid );

	eventArg->mSessionManager->put( sid.mKey, session, &sid.mSeq );

	session->setHandler( pushArg->mHandler );
	session->setArg( eventArg );

	event_set( session->getReadEvent(), pushArg->mFd, EV_READ,
			SP_EventCallback::onRead, session );
	event_set( session->getWriteEvent(), pushArg->mFd, EV_WRITE,
			SP_EventCallback::onWrite, session );

	SP_EventCallback::addEvent( session, EV_WRITE, pushArg->mFd );
	SP_EventCallback::addEvent( session, EV_READ, pushArg->mFd );

	if( pushArg->mNeedStart ) SP_EventHelper::doStart( session );

	free( pushArg );
}

int SP_Dispatcher :: push( int fd, SP_Handler * handler, int needStart )
{
	uint16_t seq = 0;
	if( NULL != mEventArg->mSessionManager->get( fd, &seq ) ) {
		return -1;
	}

	SP_PushArg_t * arg = (SP_PushArg_t*)malloc( sizeof( SP_PushArg_t ) );
	arg->mFd = fd;
	arg->mHandler = handler;
	arg->mNeedStart = needStart;

	SP_EventHelper::setNonblock( fd );

	return msgqueue_push( (struct event_msgqueue*)mPushQueue, arg );
}

