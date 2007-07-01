/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __speventcb_hpp__
#define __speventcb_hpp__

#include <netinet/in.h>

class SP_HandlerFactory;
class SP_Executor;
class SP_SessionManager;
class SP_Session;
class SP_CompletionHandler;
class SP_Message;

struct event_base;

typedef struct tagSP_EventArg {
	struct event_base * mEventBase;
	void * mResponseQueue;

	SP_Executor * mExecutor;
	SP_Executor * mCompletionExecutor;

	SP_CompletionHandler * mCompletionHandler;
	SP_SessionManager * mSessionManager;

	int mTimeout;
} SP_EventArg_t;

typedef struct tagSP_AcceptArg {
	SP_EventArg_t mEventArg;

	SP_HandlerFactory * mHandlerFactory;
	int mReqQueueSize;
	int mMaxConnections;
	char * mRefusedMsg;
} SP_AcceptArg_t;

class SP_EventCallback {
public:
	static void onAccept( int fd, short events, void * arg );
	static void onRead( int fd, short events, void * arg );
	static void onWrite( int fd, short events, void * arg );

	static void onResponse( void * queueData, void * arg );

	static void addEvent( SP_Session * session, short events, int fd );

private:
	SP_EventCallback();
	~SP_EventCallback();
};

class SP_EventHelper {
public:
	static void doStart( SP_Session * session );
	static void start( void * arg );

	static void doWork( SP_Session * session );
	static void worker( void * arg );

	static void doError( SP_Session * session );
	static void error( void * arg );

	static void doTimeout( SP_Session * session );
	static void timeout( void * arg );

	static void doCompletion( SP_EventArg_t * eventArg, SP_Message * msg );
	static void completion( void * arg );

	static int transmit( SP_Session * session, int fd );

	static void inetNtoa( in_addr * addr, char * ip, int size );

	static int setNonblock( int fd );

	static int tcpListen( const char * ip, int port, int * fd, int blocking = 1 );

private:
	SP_EventHelper();
	~SP_EventHelper();
};

#endif

