/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */


#ifndef __spthreadpool_hpp__
#define __spthreadpool_hpp__

#include <pthread.h>

typedef struct tagSP_Thread SP_Thread_t;

class SP_ThreadPool {
public:
	typedef void ( * DispatchFunc_t )( void * );

	SP_ThreadPool( int maxThreads, const char * tag = 0 );
	~SP_ThreadPool();

	/// @return 0 : OK, -1 : cannot create thread
	int dispatch( DispatchFunc_t dispatchFunc, void *arg );

	int getMaxThreads();

private:
	char * mTag;

	int mMaxThreads;
	int mIndex;
	int mTotal;
	int mIsShutdown;

	pthread_mutex_t mMainMutex;
	pthread_cond_t mIdleCond;
	pthread_cond_t mFullCond;
	pthread_cond_t mEmptyCond;

	SP_Thread_t ** mThreadList;

	static void * wrapperFunc( void * );
	int saveThread( SP_Thread_t * thread );
};

#endif

