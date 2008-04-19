/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "spporting.hpp"

#include "spthreadpool.hpp"

typedef struct tagSP_Thread {
	pthread_t mId;
	pthread_mutex_t mMutex;
	pthread_cond_t mCond;
	SP_ThreadPool::DispatchFunc_t mFunc;
	void * mArg;
	SP_ThreadPool * mParent;
} SP_Thread_t;

SP_ThreadPool :: SP_ThreadPool( int maxThreads, const char * tag )
{
	if( maxThreads <= 0 ) maxThreads = 2;

	pthread_mutex_init( &mMainMutex, NULL );
	pthread_cond_init( &mIdleCond, NULL );
	pthread_cond_init( &mFullCond, NULL );
	pthread_cond_init( &mEmptyCond, NULL );
	mMaxThreads = maxThreads;
	mIndex = 0;
	mIsShutdown = 0;
	mTotal = 0;

	tag = NULL == tag ? "unknown" : tag;
	mTag = strdup( tag );

	mThreadList = ( SP_Thread_t ** )malloc( sizeof( void * ) * mMaxThreads );
	memset( mThreadList, 0, sizeof( void * ) * mMaxThreads );
}

SP_ThreadPool :: ~SP_ThreadPool()
{
	pthread_mutex_lock( &mMainMutex );

	if( mIndex < mTotal ) {
		sp_syslog( LOG_NOTICE, "[tp@%s] waiting for %d thread(s) to finish\n", mTag, mTotal - mIndex );
		pthread_cond_wait( &mFullCond, &mMainMutex );
	}

	mIsShutdown = 1;

	int i = 0;

	for( i = 0; i < mIndex; i++ ) {
		SP_Thread_t * thread = mThreadList[ i ];
		pthread_mutex_lock( &thread->mMutex );
		pthread_cond_signal( &thread->mCond ) ;
		pthread_mutex_unlock ( &thread->mMutex );
	}

	if( mTotal > 0 ) {
		sp_syslog( LOG_NOTICE, "[tp@%s] waiting for %d thread(s) to exit\n", mTag, mTotal );
		pthread_cond_wait( &mEmptyCond, &mMainMutex );
	}

	sp_syslog( LOG_NOTICE, "[tp@%s] destroy %d thread structure(s)\n", mTag, mIndex );
	for( i = 0; i < mIndex; i++ ) {
		SP_Thread_t * thread = mThreadList[ i ];
		pthread_mutex_destroy( &thread->mMutex );
		pthread_cond_destroy( &thread->mCond );
		free( thread );
		mThreadList[ i ] = NULL;
	}

	pthread_mutex_unlock( &mMainMutex );

	mIndex = 0;

	pthread_mutex_destroy( &mMainMutex );
	pthread_cond_destroy( &mIdleCond );
	pthread_cond_destroy( &mFullCond );
	pthread_cond_destroy( &mEmptyCond );

	free( mThreadList );
	mThreadList = NULL;

	free( mTag );
	mTag = NULL;
}

int SP_ThreadPool :: getMaxThreads()
{
	return mMaxThreads;
}

int SP_ThreadPool :: dispatch( DispatchFunc_t dispatchFunc, void *arg )
{
	int ret = 0;

	pthread_attr_t attr;
	SP_Thread_t * thread = NULL;

	pthread_mutex_lock( &mMainMutex );

	if( mIndex <= 0 && mTotal >= mMaxThreads ) {
		pthread_cond_wait( &mIdleCond, &mMainMutex );
	}

	if( mIndex <= 0 ) {
		SP_Thread_t * thread = ( SP_Thread_t * )malloc( sizeof( SP_Thread_t ) );
		memset( &thread->mId, 0, sizeof( thread->mId ) );
		pthread_mutex_init( &thread->mMutex, NULL );
		pthread_cond_init( &thread->mCond, NULL );
		thread->mFunc = dispatchFunc;
		thread->mArg = arg;
		thread->mParent = this;

		pthread_attr_init( &attr );
		pthread_attr_setdetachstate( &attr,PTHREAD_CREATE_DETACHED );

		if( 0 == pthread_create( &( thread->mId ), &attr, wrapperFunc, thread ) ) {
			mTotal++;
			sp_syslog( LOG_NOTICE, "[tp@%s] create thread#%ld\n", mTag, thread->mId );
		} else {
			ret = -1;
			sp_syslog( LOG_WARNING, "[tp@%s] cannot create thread\n", mTag );
			pthread_mutex_destroy( &thread->mMutex );
			pthread_cond_destroy( &thread->mCond );
			free( thread );
		}
		pthread_attr_destroy( &attr );
	} else {
		mIndex--;
		thread = mThreadList[ mIndex ];
		mThreadList[ mIndex ] = NULL;

		thread->mFunc = dispatchFunc;
		thread->mArg = arg;
		thread->mParent = this;

		pthread_mutex_lock( &thread->mMutex );
		pthread_cond_signal( &thread->mCond ) ;
		pthread_mutex_unlock ( &thread->mMutex );
	}

	pthread_mutex_unlock( &mMainMutex );

	return ret;
}

void * SP_ThreadPool :: wrapperFunc( void * arg )
{
	SP_Thread_t * thread = ( SP_Thread_t * )arg;

	for( ; 0 == thread->mParent->mIsShutdown; ) {
		thread->mFunc( thread->mArg );

		pthread_mutex_lock( &thread->mMutex );
		if( 0 == thread->mParent->saveThread( thread ) ) {
			pthread_cond_wait( &thread->mCond, &thread->mMutex );
			pthread_mutex_unlock( &thread->mMutex );
		} else {
			pthread_mutex_unlock( &thread->mMutex );
			pthread_cond_destroy( &thread->mCond );
			pthread_mutex_destroy( &thread->mMutex );

			free( thread );
			thread = NULL;
			break;
		}
	}

	if( NULL != thread ) {
		pthread_mutex_lock( &thread->mParent->mMainMutex );
		thread->mParent->mTotal--;
		if( thread->mParent->mTotal <= 0 ) {
			pthread_cond_signal( &thread->mParent->mEmptyCond );
		}
		pthread_mutex_unlock( &thread->mParent->mMainMutex );
	}

	return NULL;
}

int SP_ThreadPool :: saveThread( SP_Thread_t * thread )
{
	int ret = -1;

	pthread_mutex_lock( &mMainMutex );

	if( mIndex < mMaxThreads ) {
		mThreadList[ mIndex ] = thread;
		mIndex++;
		ret = 0;

		pthread_cond_signal( &mIdleCond );

		if( mIndex >= mTotal ) {
			pthread_cond_signal( &mFullCond );
		}
	}

	pthread_mutex_unlock( &mMainMutex );

	return ret;
}

