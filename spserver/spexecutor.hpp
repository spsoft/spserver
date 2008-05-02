/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */


#ifndef __spexecutor_hpp__
#define __spexecutor_hpp__

#include <pthread.h>

class SP_ThreadPool;
class SP_BlockingQueue;

class SP_Task {
public:
	virtual ~SP_Task();
	virtual void run() = 0;
};

class SP_SimpleTask : public SP_Task {
public:
	typedef void ( * ThreadFunc_t ) ( void * );

	SP_SimpleTask( ThreadFunc_t func, void * arg, int deleteAfterRun );
	virtual ~SP_SimpleTask();

	virtual void run();

private:
	ThreadFunc_t mFunc;
	void * mArg;

	int mDeleteAfterRun;
};

class SP_Executor {
public:
	SP_Executor( int maxThreads, const char * tag = 0 );
	~SP_Executor();

	void execute( SP_Task * task );
	void execute( void ( * func ) ( void * ), void * arg );
	int getQueueLength();
	void shutdown();

private:
	static void msgQueueCallback( void * queueData, void * arg );
	static void worker( void * arg );
	static void * eventLoop( void * arg );

	SP_ThreadPool * mThreadPool;
	SP_BlockingQueue * mQueue;

	int mIsShutdown;

	pthread_mutex_t mMutex;
	pthread_cond_t mCond;
};

#endif

