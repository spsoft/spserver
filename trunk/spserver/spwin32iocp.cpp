/*
 * Copyright 2008 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <assert.h>

#include "spwin32iocp.hpp"

#include "spsession.hpp"
#include "spbuffer.hpp"
#include "spmsgdecoder.hpp"
#include "sprequest.hpp"
#include "sputils.hpp"
#include "sphandler.hpp"
#include "spexecutor.hpp"
#include "spioutils.hpp"
#include "spmsgblock.hpp"

BOOL SP_IocpEventCallback :: addSession( SP_IocpEventArg * eventArg, HANDLE client, SP_Session * session )
{
	BOOL ret = TRUE;

	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)malloc( sizeof( SP_IocpSession_t ) );
	if( NULL == iocpSession ) {
		sp_syslog( LOG_ERR, "malloc fail, errno %d", GetLastError() );
		ret = FALSE;
	}

	DWORD completionKey = 0;
	SP_Sid_t sid = session->getSid();
	assert( sizeof( completionKey ) == sizeof( SP_Sid_t ) );
	memcpy( &completionKey, &sid, sizeof( completionKey ) );

	if( ret ) {
		memset( iocpSession, 0, sizeof( SP_IocpSession_t ) );
		iocpSession->mRecvEvent.mHeapIndex = -1;
		iocpSession->mSendEvent.mHeapIndex = -1;

		iocpSession->mHandle = client;
		iocpSession->mSession = session;
		iocpSession->mEventArg = eventArg;
		session->setArg( iocpSession );

		if( NULL == CreateIoCompletionPort( client, eventArg->getCompletionPort(), completionKey, 0 ) ) {
			sp_syslog( LOG_ERR, "CreateIoCompletionPort fail, errno %d", WSAGetLastError() );
			ret = FALSE;
		}
	}

	if( ret ) ret = addRecv( session );

	if( ! ret ) {
		sp_close( (SOCKET)client );

		if( NULL != iocpSession ) free( iocpSession );
		session->setArg( NULL );
	}

	return ret;

}

BOOL SP_IocpEventCallback :: addRecv( SP_Session * session )
{
	BOOL ret = true;

	if( 0 == session->getReading() ) {
		SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();

		memset( &( iocpSession->mRecvEvent.mOverlapped ), 0, sizeof( OVERLAPPED ) );
		iocpSession->mRecvEvent.mType = SP_IocpEvent_t::eEventRecv;
		iocpSession->mRecvEvent.mWsaBuf[0].buf = iocpSession->mBuffer;
		iocpSession->mRecvEvent.mWsaBuf[0].len = sizeof( iocpSession->mBuffer );

		DWORD recvBytes = 0, flags = 0;
		if( SOCKET_ERROR == WSARecv( (SOCKET)iocpSession->mHandle, iocpSession->mRecvEvent.mWsaBuf, 1,
				&recvBytes, &flags, &( iocpSession->mRecvEvent.mOverlapped ), NULL ) ) {
			if( ERROR_IO_PENDING != WSAGetLastError() ) {
				sp_syslog( LOG_ERR, "WSARecv fail, errno %d", WSAGetLastError() );
				ret = FALSE;
			}
		}

		if( ret ) {
			iocpSession->mSession->setReading( 1 );

			SP_IocpEventArg * eventArg = iocpSession->mEventArg;

			if( eventArg->getTimeout() > 0 ) {
				SP_IocpEvent_t * recvEvent = &( iocpSession->mRecvEvent );
				sp_gettimeofday( &( recvEvent->mTimeout ), NULL );
				recvEvent->mTimeout.tv_sec += eventArg->getTimeout();
				eventArg->getEventHeap()->push( recvEvent );
			}
		}
	}

	return ret;
}

BOOL SP_IocpEventCallback :: addSend( SP_Session * session )
{
	BOOL ret = TRUE;

	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();

	if( 0 == session->getWriting() ) {
		if( session->getOutList()->getCount() > 0 ) {
			SP_Message * msg = (SP_Message*)session->getOutList()->getItem(0);
			iocpSession->mSendEvent.mWsaBuf[0].buf = (char*)msg->getMsg()->getBuffer();
			iocpSession->mSendEvent.mWsaBuf[0].len = msg->getMsg()->getSize();
	
			iocpSession->mSendEvent.mType = SP_IocpEvent_t::eEventSend;
			memset( &( iocpSession->mSendEvent.mOverlapped ), 0, sizeof( OVERLAPPED ) );
		
			DWORD sendBytes;

			if( SOCKET_ERROR == WSASend( (SOCKET)iocpSession->mHandle, iocpSession->mSendEvent.mWsaBuf, 1,
					&sendBytes, 0,	&( iocpSession->mSendEvent.mOverlapped ), NULL ) ) {
				if( ERROR_IO_PENDING != WSAGetLastError() ) {
					sp_syslog( LOG_ERR, "WSASend fail, errno %d", WSAGetLastError() );
					ret = FALSE;
				}
			}

			if( ret ) {
				iocpSession->mSession->setWriting( 1 );

				SP_IocpEventArg * eventArg = iocpSession->mEventArg;

				if( eventArg->getTimeout() > 0 ) {
					SP_IocpEvent_t * sendEvent = &( iocpSession->mSendEvent );
					sp_gettimeofday( &( sendEvent->mTimeout ), NULL );
					sendEvent->mTimeout.tv_sec += eventArg->getTimeout();
					eventArg->getEventHeap()->push( sendEvent );
				}
			}
		}
	}

	return ret;
}

BOOL SP_IocpEventCallback :: onRecv( SP_IocpSession_t * iocpSession, int bytesTransferred )
{
	SP_IocpEvent_t * recvEvent = &( iocpSession->mRecvEvent );

	SP_Session * session = iocpSession->mSession;
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	eventArg->getEventHeap()->erase( recvEvent );

	session->setReading( 0 );

	session->getInBuffer()->append( iocpSession->mBuffer, bytesTransferred );
	if( 0 == session->getRunning() ) {
		SP_MsgDecoder * decoder = session->getRequest()->getMsgDecoder();
		if( SP_MsgDecoder::eOK == decoder->decode( session->getInBuffer() ) ) {
			SP_IocpEventHelper::doWork( session );
		}
	}

	return addRecv( session );
}

BOOL SP_IocpEventCallback :: transmit( SP_IocpSession_t * iocpSession, int bytesTransferred )
{
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;
	SP_Session * session = iocpSession->mSession;
	SP_IocpEvent_t * sendEvent = &( iocpSession->mSendEvent );

	SP_ArrayList * outList = session->getOutList();
	size_t outOffset = session->getOutOffset() + bytesTransferred;

	for( ; outList->getCount() > 0; ) {
		SP_Message * msg = (SP_Message*)outList->getItem( 0 );
		if( outOffset >= msg->getTotalSize() ) {
			msg = (SP_Message*)outList->takeItem( 0 );
			outOffset = outOffset - msg->getTotalSize();

			int index = msg->getToList()->find( session->getSid() );
			if( index >= 0 ) msg->getToList()->take( index );
			msg->getSuccess()->add( session->getSid() );

			if( msg->getToList()->getCount() <= 0 ) {
				eventArg->getOutputResultQueue()->push( msg );
			}
		} else {
			break;
		}
	}

	session->setOutOffset( outOffset );

	WSABUF * iovArray = sendEvent->mWsaBuf;

	int iovSize = 0;

	for( int i = 0; i < outList->getCount() && iovSize < SP_IocpEvent_t::SP_IOCP_MAX_IOV; i++ ) {
		SP_Message * msg = (SP_Message*)outList->getItem( i );

		if( outOffset >= msg->getMsg()->getSize() ) {
			outOffset -= msg->getMsg()->getSize();
		} else {
			iovArray[ iovSize ].buf = (char*)msg->getMsg()->getBuffer() + outOffset;
			iovArray[ iovSize++ ].len = msg->getMsg()->getSize() - outOffset;
			outOffset = 0;
		}

		SP_MsgBlockList * blockList = msg->getFollowBlockList();
		for( int j = 0; j < blockList->getCount() && iovSize < SP_IocpEvent_t::SP_IOCP_MAX_IOV; j++ ) {
			SP_MsgBlock * block = (SP_MsgBlock*)blockList->getItem( j );

			if( outOffset >= block->getSize() ) {
				outOffset -= block->getSize();
			} else {
				iovArray[ iovSize ].buf = (char*)block->getData() + outOffset;
				iovArray[ iovSize++ ].len = block->getSize() - outOffset;
				outOffset = 0;
			}
		}
	}

	if( session->getOutList()->getCount() > 0 )	{
		memset( &( sendEvent->mOverlapped ), 0, sizeof( OVERLAPPED ) );
		
		DWORD sendBytes;

		if( SOCKET_ERROR == WSASend( (SOCKET)iocpSession->mHandle, sendEvent->mWsaBuf, iovSize,
			&sendBytes, 0,	&( sendEvent->mOverlapped ), NULL ) ) {
			if( ERROR_IO_PENDING != WSAGetLastError() ) {
				sp_syslog( LOG_ERR, "WSASend fail, errno %d", WSAGetLastError() );
				return FALSE;
			}
		}

		session->setWriting( 1 );
	}

	return TRUE;
}

BOOL SP_IocpEventCallback :: onSend( SP_IocpSession_t * iocpSession, int bytesTransferred )
{
	BOOL ret = TRUE;

	SP_Session * session = iocpSession->mSession;
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_IocpEvent_t * sendEvent = &( iocpSession->mSendEvent );

	SP_Sid_t sid = session->getSid();

	eventArg->getEventHeap()->erase( sendEvent );

	session->setWriting( 0 );

	if( 0 == bytesTransferred ) {
		if( 0 == session->getRunning() ) {
			sp_syslog( LOG_NOTICE, "session(%d.%d) write error", sid.mKey, sid.mSeq );
			SP_IocpEventHelper::doError( session );
		} else {
			session->setStatus( SP_Session::eWouldExit );
		}
	} else {
		// process pending input
		if( 0 == session->getRunning() ) {
			SP_MsgDecoder * decoder = session->getRequest()->getMsgDecoder();
			if( SP_MsgDecoder::eOK == decoder->decode( session->getInBuffer() ) ) {
				SP_IocpEventHelper::doWork( session );
			}
		} else {
			// If this session is running, then onResponse will add write event for this session.
			// So no need to add write event here.
		}

		ret = transmit( iocpSession, bytesTransferred );

		if( session->getOutList()->getCount() <= 0 ) {
			if( SP_Session::eExit == session->getStatus() ) {
				if( 0 == session->getRunning() ) {
					SP_IocpEventHelper::doClose( session );
					return TRUE;
				}
			}
		}
	}

	return ret;
}

BOOL SP_IocpEventCallback :: onAccept( SP_IocpAcceptArg_t * acceptArg )
{
	SP_IocpEventArg * eventArg = acceptArg->mEventArg;

	SP_Sid_t sid;
	sid.mKey = (uint16_t) acceptArg->mClientSocket;
	eventArg->getSessionManager()->get( sid.mKey, &sid.mSeq );

	SP_Session * session = new SP_Session( sid );

	int localLen = 0, remoteLen = 0;
	struct sockaddr_in * localAddr = NULL, * remoteAddr = NULL;

	GetAcceptExSockaddrs( acceptArg->mBuffer, 0,
		sizeof( sockaddr_in ) + 16, sizeof( sockaddr_in ) + 16,
		(SOCKADDR**)&localAddr, &localLen, (SOCKADDR**)&remoteAddr, &remoteLen );

	struct sockaddr_in clientAddr;
	memcpy( &clientAddr, remoteAddr, sizeof( clientAddr ) );

	char clientIP[ 32 ] = { 0 };
	SP_IOUtils::inetNtoa( &( clientAddr.sin_addr ), clientIP, sizeof( clientIP ) );
	session->getRequest()->setClientIP( clientIP );

	session->setHandler( acceptArg->mHandlerFactory->create() );	

	if( addSession( eventArg, acceptArg->mClientSocket, session ) ) {
		eventArg->getSessionManager()->put( sid.mKey, session, &sid.mSeq );

		if( eventArg->getSessionManager()->getCount() > acceptArg->mMaxConnections
				|| eventArg->getInputResultQueue()->getLength() >= acceptArg->mReqQueueSize ) {

			sp_syslog( LOG_WARNING, "System busy, session.count %d [%d], queue.length %d [%d]",
				eventArg->getSessionManager()->getCount(), acceptArg->mMaxConnections,
				eventArg->getInputResultQueue()->getLength(), acceptArg->mReqQueueSize );

			SP_Message * msg = new SP_Message();
			msg->getMsg()->append( acceptArg->mRefusedMsg );
			msg->getMsg()->append( "\r\n" );
			session->getOutList()->append( msg );
			session->setStatus( SP_Session::eExit );

			addSend( session );
		} else {
			SP_IocpEventHelper::doStart( session );
		}
	} else {
		delete session;
	}

	// signal SP_IocpServer::acceptThread to post another AcceptEx
	SetEvent( acceptArg->mAcceptEvent );

	return TRUE;
}

void SP_IocpEventCallback :: onResponse( void * queueData, void * arg )
{
	SP_Response * response = (SP_Response*)queueData;
	SP_IocpEventArg * eventArg = (SP_IocpEventArg*)arg;
	SP_SessionManager * manager = eventArg->getSessionManager();

	SP_Sid_t fromSid = response->getFromSid();
	uint16_t seq = 0;

	if( ! SP_IocpEventHelper::isSystemSid( &fromSid ) ) {
		SP_Session * session = manager->get( fromSid.mKey, &seq );
		if( seq == fromSid.mSeq && NULL != session ) {
			if( SP_Session::eWouldExit == session->getStatus() ) {
				session->setStatus( SP_Session::eExit );
			}

			if( SP_Session::eNormal == session->getStatus() ) {
				addRecv( session );
			}

			// always add a write event for sender, 
			// so the pending input can be processed in onWrite
			addSend( session );
		} else {
			sp_syslog( LOG_WARNING, "session(%d.%d) invalid, unknown FROM",
					fromSid.mKey, fromSid.mSeq );
		}
	}

	for( SP_Message * msg = response->takeMessage();
			NULL != msg; msg = response->takeMessage() ) {

		SP_SidList * sidList = msg->getToList();

		if( msg->getTotalSize() > 0 ) {
			for( int i = sidList->getCount() - 1; i >= 0; i-- ) {
				SP_Sid_t sid = sidList->get( i );
				SP_Session * session = manager->get( sid.mKey, &seq );
				if( seq == sid.mSeq && NULL != session ) {
					if( 0 != memcmp( &fromSid, &sid, sizeof( sid ) )
							&& SP_Session::eExit == session->getStatus() ) {
						sidList->take( i );
						msg->getFailure()->add( sid );
						sp_syslog( LOG_WARNING, "session(%d.%d) would exit, invalid TO", sid.mKey, sid.mSeq );
					} else {
						session->getOutList()->append( msg );
						addSend( session );
					}
				} else {
					sidList->take( i );
					msg->getFailure()->add( sid );
					sp_syslog( LOG_WARNING, "session(%d.%d) invalid, unknown TO", sid.mKey, sid.mSeq );
				}
			}
		} else {
			for( ; sidList->getCount() > 0; ) {
				msg->getFailure()->add( sidList->take( SP_ArrayList::LAST_INDEX ) );
			}
		}

		if( msg->getToList()->getCount() <= 0 ) {
			SP_IocpEventHelper::doCompletion( eventArg, msg );
		}
	}

	delete response;
}

void SP_IocpEventCallback :: onTimeout( SP_IocpEventArg * eventArg )
{
	SP_IocpEventHeap * eventHeap = eventArg->getEventHeap();

	if( NULL == eventHeap->top() ) return;
	
	struct timeval curr;
	sp_gettimeofday( &curr, NULL );

	for( ; NULL != eventHeap->top(); ) {
		SP_IocpEvent_t * event = eventHeap->top();
		struct timeval * first = &( event->mTimeout );

		if( ( curr.tv_sec == first->tv_sec && curr.tv_usec >= first->tv_usec )
				||( curr.tv_sec > first->tv_sec ) ) {
			event = eventHeap->pop();

			SP_IocpSession_t * iocpSession = NULL;

			if( SP_IocpEvent_t::eEventRecv == event->mType ) {
				iocpSession = CONTAINING_RECORD( event, SP_IocpSession_t, mRecvEvent );
			} else if( SP_IocpEvent_t::eEventSend == event->mType ) {
				iocpSession = CONTAINING_RECORD( event, SP_IocpSession_t, mSendEvent );
			}

			assert( NULL != iocpSession );

			SP_IocpEventHelper::doTimeout( iocpSession->mSession );
		} else {
			break;
		}
	}
}

BOOL SP_IocpEventCallback :: eventLoop( SP_IocpEventArg * eventArg, SP_IocpAcceptArg_t * acceptArg )
{
	DWORD bytesTransferred = 0;
	DWORD completionKey = 0;
	OVERLAPPED * overlapped = NULL;
	HANDLE completionPort = eventArg->getCompletionPort();
	DWORD timeout = SP_IocpEventHelper::timeoutNext( eventArg->getEventHeap() );

	BOOL isSuccess = GetQueuedCompletionStatus( completionPort, &bytesTransferred,
			&completionKey, &overlapped, timeout );
	DWORD lastError = WSAGetLastError();

	SP_IocpSession_t * iocpSession = NULL;
	if( completionKey > 0 ) {
		SP_Sid_t sid;
		memcpy( &sid, &completionKey, sizeof( completionKey ) );

		uint16_t seq = 0;
		SP_Session * session = eventArg->getSessionManager()->get( sid.mKey, &seq );
		if( NULL != session && sid.mSeq == seq ) {
			iocpSession = (SP_IocpSession_t*)session->getArg();
		}
	}

	if( ! isSuccess ) {
		if( NULL != overlapped ) {
			// process a failed completed I/O request
			// lastError continas the reason for failure

			if( NULL != iocpSession ) {
				SP_IocpEventHelper::doClose( iocpSession->mSession );
			}

			if( ERROR_NETNAME_DELETED == WSAGetLastError() // client abort
					|| ERROR_OPERATION_ABORTED == WSAGetLastError() ) {
				return TRUE;
			} else {
				char errmsg[ 512 ] = { 0 };
				spwin32_strerror( WSAGetLastError(), errmsg, sizeof( errmsg ) );
				sp_syslog( LOG_ERR, "GetQueuedCompletionStatus fail, errno %d, %s",
						WSAGetLastError(), errmsg );
				return FALSE;
			}
		} else {
			if( lastError == WAIT_TIMEOUT ) {
				// time-out while waiting for completed I/O request
				onTimeout( eventArg );
			} else {
				// bad call to GQCS, lastError contains the reason for the bad call
			}
		}

		return FALSE;
	}

	if( eKeyAccept == completionKey ) {
		return onAccept( acceptArg );
	} else if( eKeyResponse == completionKey ) {
		SP_IocpMsgQueue_t * msgQueue = eventArg->getResponseQueue();

		for( ; msgQueue->mQueue->getLength() > 0; ) {
			SP_Response * response = NULL;

			EnterCriticalSection( &( msgQueue->mMutex ) );
			response = (SP_Response*)msgQueue->mQueue->pop();
			LeaveCriticalSection( &( msgQueue->mMutex ) );

			onResponse( response, eventArg );
		}
		return TRUE;
	} else if( eKeyFree == completionKey ) {
		assert( NULL == iocpSession );
		iocpSession = CONTAINING_RECORD( overlapped, SP_IocpSession_t, mFreeEvent );
		delete iocpSession->mSession;
		free( iocpSession );
		return TRUE;
	} else {
		if( NULL == iocpSession ) return TRUE;

		if( bytesTransferred == 0 )	{
			if( NULL != iocpSession ) {
				SP_IocpEventHelper::doClose( iocpSession->mSession );
			}
			return TRUE;
		}

		SP_IocpEvent_t * iocpEvent = 
				CONTAINING_RECORD( overlapped, SP_IocpEvent_t, mOverlapped );

		eventArg->getEventHeap()->erase( iocpEvent );

		if( SP_IocpEvent_t::eEventRecv == iocpEvent->mType ) {
			if( ! onRecv( iocpSession, bytesTransferred ) ) {
				SP_IocpEventHelper::doError( iocpSession->mSession );
			}
			return TRUE;
		}

		if( SP_IocpEvent_t::eEventSend == iocpEvent->mType ) {
			if( ! onSend( iocpSession, bytesTransferred ) ) {
				SP_IocpEventHelper::doError( iocpSession->mSession );
			}
			return TRUE;
		}
	}

	return TRUE;
}

//===================================================================

int SP_IocpEventHelper :: isSystemSid( SP_Sid_t * sid )
{
	return sid->mKey == SP_Sid_t::eTimerKey && sid->mSeq == SP_Sid_t::eTimerSeq;
}

DWORD SP_IocpEventHelper :: timeoutNext( SP_IocpEventHeap * eventHeap )
{
	SP_IocpEvent_t * event = eventHeap->top();

	if( NULL == event ) return INFINITE;

	struct timeval curr;
	sp_gettimeofday( &curr, NULL );

	struct timeval * first = &( event->mTimeout );

	DWORD ret = ( first->tv_sec - curr.tv_sec ) * 1000
		+ ( first->tv_usec - curr.tv_usec ) / 1000;

	if( ret < 0 ) ret = 1;

	return ret;
}

void SP_IocpEventHelper :: doWork( SP_Session * session )
{
	if( SP_Session::eNormal == session->getStatus() ) {
		session->setRunning( 1 );
		SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
		SP_IocpEventArg * eventArg = iocpSession->mEventArg;
		eventArg->getInputResultQueue()->push( new SP_SimpleTask( worker, session, 1 ) );
	} else {
		SP_Sid_t sid = session->getSid();

		char buffer[ 16 ] = { 0 };
		session->getInBuffer()->take( buffer, sizeof( buffer ) );
		sp_syslog( LOG_WARNING, "session(%d.%d) status is %d, ignore [%s...] (%dB)",
			sid.mKey, sid.mSeq, session->getStatus(), buffer, session->getInBuffer()->getSize() );
		session->getInBuffer()->reset();
	}
}

void SP_IocpEventHelper :: worker( void * arg )
{
	SP_Session * session = (SP_Session*)arg;
	SP_Handler * handler = session->getHandler();
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Response * response = new SP_Response( session->getSid() );
	if( 0 != handler->handle( session->getRequest(), response ) ) {
		session->setStatus( SP_Session::eWouldExit );
	}

	session->setRunning( 0 );

	enqueue( eventArg, response );
}


void SP_IocpEventHelper :: doClose( SP_Session * session )
{
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Sid_t sid = session->getSid();

	if( 0 == session->getRunning() ) {
		session->setRunning( 1 );

		// remove session from SessionManager, the other threads will ignore this session
		eventArg->getSessionManager()->remove( sid.mKey );

		eventArg->getEventHeap()->erase( &( iocpSession->mRecvEvent ) );
		eventArg->getEventHeap()->erase( &( iocpSession->mSendEvent ) );

		eventArg->getInputResultQueue()->push( new SP_SimpleTask( close, session, 1 ) );
	} else {
		sp_syslog( LOG_DEBUG, "DEBUG: session(%d,%d) is busy", sid.mKey, sid.mSeq );
	}
}

void SP_IocpEventHelper :: close( void * arg )
{
	SP_Session * session = (SP_Session*)arg;
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Sid_t sid = session->getSid();

	//sp_syslog( LOG_NOTICE, "session(%d.%d) close, disconnect", sid.mKey, sid.mSeq );

	session->getHandler()->close();

	if( ! eventArg->disconnectEx( (SOCKET)iocpSession->mHandle ) ) {
		if( ERROR_IO_PENDING != WSAGetLastError () ) {
			sp_syslog( LOG_ERR, "DisconnectEx(%d) fail, errno %d", sid.mKey, WSAGetLastError() );
		}
	}

	if( 0 != sp_close( (SOCKET)iocpSession->mHandle ) ) {
		sp_syslog( LOG_ERR, "close(%d) fail, errno %d", sid.mKey, WSAGetLastError() );
	}

	memset( &( iocpSession->mFreeEvent ), 0, sizeof( OVERLAPPED ) );
	PostQueuedCompletionStatus( eventArg->getCompletionPort(), 0,
			SP_IocpEventCallback::eKeyFree, &( iocpSession->mFreeEvent ) );

	sp_syslog( LOG_NOTICE, "session(%d.%d) close, exit", sid.mKey, sid.mSeq );
}

void SP_IocpEventHelper :: doError( SP_Session * session )
{
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	session->setRunning( 1 );

	sp_close( (SOCKET)iocpSession->mHandle );

	SP_Sid_t sid = session->getSid();

	SP_ArrayList * outList = session->getOutList();
	for( ; outList->getCount() > 0; ) {
		SP_Message * msg = ( SP_Message * ) outList->takeItem( SP_ArrayList::LAST_INDEX );

		int index = msg->getToList()->find( sid );
		if( index >= 0 ) msg->getToList()->take( index );
		msg->getFailure()->add( sid );

		if( msg->getToList()->getCount() <= 0 ) {
			doCompletion( eventArg, msg );
		}
	}

	// remove session from SessionManager, so the other threads will ignore this session
	eventArg->getSessionManager()->remove( sid.mKey );

	eventArg->getEventHeap()->erase( &( iocpSession->mRecvEvent ) );
	eventArg->getEventHeap()->erase( &( iocpSession->mSendEvent ) );

	eventArg->getInputResultQueue()->push( new SP_SimpleTask( error, session, 1 ) );
}

void SP_IocpEventHelper :: error( void * arg )
{
	SP_Session * session = ( SP_Session * )arg;
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Sid_t sid = session->getSid();

	SP_Response * response = new SP_Response( sid );
	session->getHandler()->error( response );

	enqueue( eventArg, response );

	// the other threads will ignore this session, so it's safe to destroy session here
	session->getHandler()->close();

	memset( &( iocpSession->mFreeEvent ), 0, sizeof( OVERLAPPED ) );
	PostQueuedCompletionStatus( eventArg->getCompletionPort(), 0,
			SP_IocpEventCallback::eKeyFree, &( iocpSession->mFreeEvent ) );

	sp_syslog( LOG_WARNING, "session(%d.%d) error, exit", sid.mKey, sid.mSeq );
}

void SP_IocpEventHelper :: doTimeout( SP_Session * session )
{
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	session->setRunning( 1 );

	sp_close( (SOCKET)iocpSession->mHandle );

	SP_Sid_t sid = session->getSid();

	SP_ArrayList * outList = session->getOutList();
	for( ; outList->getCount() > 0; ) {
		SP_Message * msg = ( SP_Message * ) outList->takeItem( SP_ArrayList::LAST_INDEX );

		int index = msg->getToList()->find( sid );
		if( index >= 0 ) msg->getToList()->take( index );
		msg->getFailure()->add( sid );

		if( msg->getToList()->getCount() <= 0 ) {
			doCompletion( eventArg, msg );
		}
	}

	// remove session from SessionManager, the other threads will ignore this session
	eventArg->getSessionManager()->remove( sid.mKey );

	eventArg->getEventHeap()->erase( &( iocpSession->mRecvEvent ) );
	eventArg->getEventHeap()->erase( &( iocpSession->mSendEvent ) );

	eventArg->getInputResultQueue()->push( new SP_SimpleTask( timeout, session, 1 ) );
}

void SP_IocpEventHelper :: timeout( void * arg )
{
	SP_Session * session = ( SP_Session * )arg;
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Sid_t sid = session->getSid();

	SP_Response * response = new SP_Response( sid );
	session->getHandler()->timeout( response );

	enqueue( eventArg, response );

	// the other threads will ignore this session, so it's safe to destroy session here
	session->getHandler()->close();

	memset( &( iocpSession->mFreeEvent ), 0, sizeof( OVERLAPPED ) );
	PostQueuedCompletionStatus( eventArg->getCompletionPort(), 0,
			SP_IocpEventCallback::eKeyFree, &( iocpSession->mFreeEvent ) );

	sp_syslog( LOG_WARNING, "session(%d.%d) timeout, exit", sid.mKey, sid.mSeq );
}

void SP_IocpEventHelper :: doStart( SP_Session * session )
{
	session->setRunning( 1 );

	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;
	eventArg->getInputResultQueue()->push( new SP_SimpleTask( start, session, 1 ) );
}

void SP_IocpEventHelper :: start( void * arg )
{
	SP_Session * session = ( SP_Session * )arg;
	SP_IocpSession_t * iocpSession = (SP_IocpSession_t*)session->getArg();
	SP_IocpEventArg * eventArg = iocpSession->mEventArg;

	SP_Response * response = new SP_Response( session->getSid() );
	int startRet = session->getHandler()->start( session->getRequest(), response );

	session->setStatus( 0 == startRet ? SP_Session::eNormal : SP_Session::eWouldExit );
	session->setRunning( 0 );

	enqueue( eventArg, response );
}

void SP_IocpEventHelper :: doCompletion( SP_IocpEventArg * eventArg, SP_Message * msg )
{
	eventArg->getOutputResultQueue()->push( msg );
}

void SP_IocpEventHelper :: enqueue( SP_IocpEventArg * eventArg, SP_Response * response )
{
	SP_IocpMsgQueue_t * msgQueue = eventArg->getResponseQueue();

	EnterCriticalSection( &( msgQueue->mMutex ) );
	msgQueue->mQueue->push( response );
	if( msgQueue->mQueue->getLength() == 1 ) {
		PostQueuedCompletionStatus( eventArg->getCompletionPort(), 0,
			SP_IocpEventCallback::eKeyResponse, NULL );
	}
	LeaveCriticalSection( &( msgQueue->mMutex ) );
}
