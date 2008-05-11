/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spiocpserver_hpp__
#define __spiocpserver_hpp__

class SP_HandlerFactory;
class SP_Session;
class SP_Executor;

class SP_IocpServer {
public:
	SP_IocpServer( const char * bindIP, int port, SP_HandlerFactory * handlerFactory );
	~SP_IocpServer();

	void setTimeout( int timeout );
	void setMaxConnections( int maxConnections );
	void setMaxThreads( int maxThreads );
	void setReqQueueSize( int reqQueueSize, const char * refusedMsg );

	void shutdown();
	int isRunning();
	int run();
	void runForever();

private:
	SP_HandlerFactory * mHandlerFactory;

	char mBindIP[ 64 ];
	int mPort;
	int mIsShutdown;
	int mIsRunning;

	int mTimeout;
	int mMaxThreads;
	int mMaxConnections;
	int mReqQueueSize;
	char * mRefusedMsg;

	static void * acceptThread( void * arg );

	static void * eventLoop( void * arg );

	int start();

	static void sigHandler( int, short, void * arg );

	static void outputCompleted( void * arg );
};

#endif
