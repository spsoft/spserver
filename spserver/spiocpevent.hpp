/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spiocpevent_hpp__
#define __spiocpevent_hpp__

#include "spporting.hpp"

typedef struct tagSP_IocpEvent {
	enum { SP_IOCP_MAX_IOV = 8 };
	enum { eEventRecv, eEventSend };

	OVERLAPPED mOverlapped;
	WSABUF mWsaBuf[ SP_IOCP_MAX_IOV ];
	int mType;

	int mHeapIndex;
	struct timeval mTimeout;
} SP_IocpEvent_t;

class SP_IocpEventHeap {
public:
	SP_IocpEventHeap();
	~SP_IocpEventHeap();

	int push( SP_IocpEvent_t * item );

	SP_IocpEvent_t * top();

	SP_IocpEvent_t * pop();

	int erase( SP_IocpEvent_t * item );

	int getCount();

private:

	static int isGreater( SP_IocpEvent_t * item1, SP_IocpEvent_t * item2 );

	int reserve( int count );

	void shiftUp( int index, SP_IocpEvent_t * item );
	void shiftDown( int index, SP_IocpEvent_t * item );

	SP_IocpEvent_t ** mEntries;
	int mMaxCount, mCount;
};

class SP_BlockingQueue;
class SP_SessionManager;

typedef struct tagSP_IocpMsgQueue {
	CRITICAL_SECTION mMutex;
	SP_BlockingQueue * mQueue;
} SP_IocpMsgQueue_t;

class SP_IocpEventArg {
public:
	SP_IocpEventArg( int timeout );
	~SP_IocpEventArg();

	HANDLE getCompletionPort();
	SP_BlockingQueue * getInputResultQueue();
	SP_BlockingQueue * getOutputResultQueue();
	SP_IocpMsgQueue_t * getResponseQueue();

	SP_SessionManager * getSessionManager();

	SP_IocpEventHeap * getEventHeap();

	int loadDisconnectEx( SOCKET fd );

	BOOL disconnectEx( SOCKET fd );

	void setTimeout( int timeout );
	int getTimeout();

private:
	SP_BlockingQueue * mInputResultQueue;
	SP_BlockingQueue * mOutputResultQueue;
	SP_IocpMsgQueue_t * mResponseQueue;

	SP_SessionManager * mSessionManager;

	SP_IocpEventHeap * mEventHeap;

	void * mDisconnectExFunc;

	int mTimeout;

	HANDLE mCompletionPort;
};

#endif

