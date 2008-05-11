/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spwin32iocp_hpp__
#define __spwin32iocp_hpp__

#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>
#include <stdio.h>

class SP_BlockingQueue;
class SP_SessionManager;
class SP_HandlerFactory;
class SP_Session;
class SP_Message;
class SP_Response;

typedef struct tagSP_Sid SP_Sid_t;

typedef struct tagSP_IocpMsgQueue SP_IocpMsgQueue_t;

class SP_IocpEventArg {
public:
	SP_IocpEventArg( int timeout );
	~SP_IocpEventArg();

	HANDLE getCompletionPort();
	SP_BlockingQueue * getInputResultQueue();
	SP_BlockingQueue * getOutputResultQueue();
	SP_IocpMsgQueue_t * getResponseQueue();

	SP_SessionManager * getSessionManager();

	int loadDisconnectEx( SOCKET fd );

	BOOL disconnectEx( SOCKET fd );

	void setTimeout( int timeout );
	int getTimeout();

private:
	SP_BlockingQueue * mInputResultQueue;
	SP_BlockingQueue * mOutputResultQueue;
	SP_IocpMsgQueue_t * mResponseQueue;

	SP_SessionManager * mSessionManager;

	void * mDisconnectExFunc;

	int mTimeout;

	HANDLE mCompletionPort;
};

typedef struct tagSP_IocpAcceptArg {
	SP_HandlerFactory * mHandlerFactory;

	int mReqQueueSize;
	int mMaxConnections;
	char * mRefusedMsg;

	// per handle data
	SP_IocpEventArg * mEventArg;
	HANDLE mListenSocket;

	// per io data
	OVERLAPPED mOverlapped;
	HANDLE mClientSocket;
	char mBuffer[ 1024 ];

	HANDLE mAcceptEvent;
} SP_IocpAcceptArg_t;

typedef struct tagSP_IocpSession SP_IocpSession_t;

class SP_IocpEventCallback {
public:

	enum { eKeyAccept, eKeyResponse };

	static BOOL addSession( SP_IocpEventArg * eventArg, HANDLE client, SP_Session * session );
	static BOOL addRecv( SP_Session * session );
	static BOOL addSend( SP_Session * session );

	static BOOL onRecv( SP_IocpSession_t * iocpSession, int bytesTransferred );
	static BOOL onSend( SP_IocpSession_t * iocpSession, int bytesTransferred );
	static BOOL onAccept( SP_IocpAcceptArg_t * acceptArg );
	static void onResponse( void * queueData, void * arg );
	
	static BOOL eventLoop( SP_IocpEventArg * eventArg, SP_IocpAcceptArg_t * acceptArg );

	static BOOL transmit( SP_IocpSession_t * iocpSession, int bytesTransferred );

private:
	SP_IocpEventCallback();
	~SP_IocpEventCallback();
};

class SP_IocpEventHelper {
public:
	static void doStart( SP_Session * session );
	static void start( void * arg );

	static void doWork( SP_Session * session );
	static void worker( void * arg );

	static void doError( SP_Session * session );
	static void error( void * arg );

	static void doTimeout( SP_Session * session );
	static void timeout( void * arg );

	static void doClose( SP_Session * session );
	static void close( void * arg );

	static void doCompletion( SP_IocpEventArg * eventArg, SP_Message * msg );

	static int isSystemSid( SP_Sid_t * sid );

	static void enqueue( SP_IocpEventArg * eventArg, SP_Response * response );

	static void getErrMsg( DWORD lastError, char * errmsg, size_t len );

private:
	SP_IocpEventHelper();
	~SP_IocpEventHelper();
};

typedef struct tagSP_IocpEvent {
	enum { SP_IOCP_MAX_IOV = 8 };
	enum { eEventRecv, eEventSend, eEventAccept };

	OVERLAPPED mOverlapped;
	WSABUF mWsaBuf[ SP_IOCP_MAX_IOV ];
	int mType;
} SP_IocpEvent_t;

typedef struct tagSP_IocpSession {
	SP_Session * mSession;
	SP_IocpEventArg * mEventArg;

	HANDLE mHandle;

	SP_IocpEvent_t mRecvEvent;
	SP_IocpEvent_t mSendEvent;

	char mBuffer[ 4096 ];
} SP_IocpSession_t;

#endif
