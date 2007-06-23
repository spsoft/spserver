/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spdispatcher_hpp__
#define __spdispatcher_hpp__

class SP_CompletionHandler;
class SP_Handler;
class SP_Message;

typedef struct tagSP_EventArg SP_EventArg_t;

class SP_Dispatcher {
public:
	SP_Dispatcher( SP_CompletionHandler * completionHandler, int maxThreads = 64 );
	~SP_Dispatcher();

	void setTimeout( int timeout );

	int getSessionCount();
	int getReqQueueLength();

	void shutdown();
	int isRunning();

	/**
	 * @brief  create a thread to run event loop
	 * @return 0 : OK, -1 : Fail, cannot create thread
	 */
	int dispatch();

	/**
	 * @brief register a fd into dispatcher
	 * @param needStart : 1 - call handler::start, 0 - don't call handler::start
	 * @return 0 : OK, -1 : Fail, invalid fd
	 * @note  handler will be deleted by dispatcher when the session is close
	 */
	int push( int fd, SP_Handler * handler, int needStart = 1 );

private:
	int mIsShutdown;
	int mIsRunning;

	SP_EventArg_t * mEventArg;

	void * mPushQueue;

	int start();

	static void * eventLoop( void * arg );

	static void sigHandler( int, short, void * arg );

	static void onPush( void * queueData, void * arg );
};

#endif

