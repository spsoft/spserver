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

#include "spiocpevent.hpp"

class SP_HandlerFactory;
class SP_Session;
class SP_Message;
class SP_Response;

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

typedef struct tagSP_IocpSession {
	SP_Session * mSession;
	SP_IocpEventArg * mEventArg;

	HANDLE mHandle;

	SP_IocpEvent_t mRecvEvent;
	SP_IocpEvent_t mSendEvent;
	OVERLAPPED mFreeEvent;

	char mBuffer[ 4096 ];
} SP_IocpSession_t;

class SP_IocpEventCallback {
public:

	enum { eKeyAccept, eKeyResponse, eKeyFree };

	static BOOL addSession( SP_IocpEventArg * eventArg, HANDLE client, SP_Session * session );
	static BOOL addRecv( SP_Session * session );
	static BOOL addSend( SP_Session * session );

	static BOOL onRecv( SP_IocpSession_t * iocpSession, int bytesTransferred );
	static BOOL onSend( SP_IocpSession_t * iocpSession, int bytesTransferred );
	static BOOL onAccept( SP_IocpAcceptArg_t * acceptArg );
	static void onResponse( void * queueData, void * arg );

	static void onTimeout( SP_IocpEventArg * eventArg );
	
	static BOOL eventLoop( SP_IocpEventArg * eventArg, SP_IocpAcceptArg_t * acceptArg );

	static BOOL transmit( SP_IocpSession_t * iocpSession, int bytesTransferred );

private:
	SP_IocpEventCallback();
	~SP_IocpEventCallback();
};

typedef struct tagSP_Sid SP_Sid_t;

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

	static DWORD timeoutNext( SP_IocpEventHeap * eventHeap );

private:
	SP_IocpEventHelper();
	~SP_IocpEventHelper();
};

#endif
