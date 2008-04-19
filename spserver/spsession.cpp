/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spporting.hpp"

#include "spsession.hpp"
#include "sphandler.hpp"
#include "spbuffer.hpp"
#include "sputils.hpp"
#include "sprequest.hpp"
#include "spiochannel.hpp"

#include "event.h"

//-------------------------------------------------------------------

typedef struct tagSP_SessionEntry {
	uint16_t mSeq;
	SP_Session * mSession;
} SP_SessionEntry;

SP_SessionManager :: SP_SessionManager()
{
	mCount = 0;
	memset( mArray, 0, sizeof( mArray ) );
}

SP_SessionManager :: ~SP_SessionManager()
{
	for( int i = 0; i < (int)( sizeof( mArray ) / sizeof( mArray[0] ) ); i++ ) {
		SP_SessionEntry_t * list = mArray[ i ];
		if( NULL != list ) {
			SP_SessionEntry_t * iter = list;
			for( int i = 0; i < 1024; i++, iter++ ) {
				if( NULL != iter->mSession ) {
					delete iter->mSession;
					iter->mSession = NULL;
				}
			}
			free( list );
		}
	}

	memset( mArray, 0, sizeof( mArray ) );
}

int SP_SessionManager :: getCount()
{
	return mCount;
}

void SP_SessionManager :: put( uint16_t key, SP_Session * session, uint16_t * seq )
{
	int row = key / 1024, col = key % 1024;

	if( NULL == mArray[ row ] ) {
		mArray[ row ] = ( SP_SessionEntry_t * )calloc(
			1024, sizeof( SP_SessionEntry_t ) );
	}

	SP_SessionEntry_t * list = mArray[ row ];
	list[ col ].mSession = session;
	*seq = list[ col ].mSeq;

	mCount++;
}

SP_Session * SP_SessionManager :: get( uint16_t key, uint16_t * seq )
{
	int row = key / 1024, col = key % 1024;

	SP_Session * ret = NULL;

	SP_SessionEntry_t * list = mArray[ row ];
	if( NULL != list ) {
		ret = list[ col ].mSession;
		* seq = list[ col ].mSeq;
	} else {
		* seq = 0;
	}

	return ret;
}

SP_Session * SP_SessionManager :: remove( uint16_t key, uint16_t * seq )
{
	int row = key / 1024, col = key % 1024;

	SP_Session * ret = NULL;

	SP_SessionEntry_t * list = mArray[ row ];
	if( NULL != list ) {
		ret = list[ col ].mSession;
		if( NULL != seq ) * seq = list[ col ].mSeq;

		list[ col ].mSession = NULL;
		list[ col ].mSeq++;

		mCount--;
	}

	return ret;
}

//-------------------------------------------------------------------

SP_Session :: SP_Session( SP_Sid_t sid )
{
	mSid = sid;

	mReadEvent = (struct event*)malloc( sizeof( struct event ) );
	mWriteEvent = (struct event*)malloc( sizeof( struct event ) );

	mHandler = NULL;
	mArg = NULL;

	mInBuffer = new SP_Buffer();
	mRequest = new SP_Request();

	mOutOffset = 0;
	mOutList = new SP_ArrayList();

	mStatus = eNormal;
	mRunning = 0;
	mWriting = 0;
	mReading = 0;

	mIOChannel = NULL;
}

SP_Session :: ~SP_Session()
{
	free( mReadEvent );
	mReadEvent = NULL;

	free( mWriteEvent );
	mWriteEvent = NULL;

	if( NULL != mHandler ) {
		delete mHandler;
		mHandler = NULL;
	}

	delete mRequest;
	mRequest = NULL;

	delete mInBuffer;
	mInBuffer = NULL;

	delete mOutList;
	mOutList = NULL;

	if( NULL != mIOChannel ) {
		delete mIOChannel;
		mIOChannel = NULL;
	}
}

struct event * SP_Session :: getReadEvent()
{
	return mReadEvent;
}

struct event * SP_Session :: getWriteEvent()
{
	return mWriteEvent;
}

void SP_Session :: setHandler( SP_Handler * handler )
{
	mHandler = handler;
}

SP_Handler * SP_Session :: getHandler()
{
	return mHandler;
}

void SP_Session :: setArg( void * arg )
{
	mArg = arg;
}

void * SP_Session :: getArg()
{
	return mArg;
}

SP_Sid_t SP_Session :: getSid()
{
	return mSid;
}

SP_Buffer * SP_Session :: getInBuffer()
{
	return mInBuffer;
}

SP_Request * SP_Session :: getRequest()
{
	return mRequest;
}

void SP_Session :: setOutOffset( int offset )
{
	mOutOffset = offset;
}

int SP_Session :: getOutOffset()
{
	return mOutOffset;
}

SP_ArrayList * SP_Session :: getOutList()
{
	return mOutList;
}

void SP_Session :: setStatus( int status )
{
	mStatus = status;
}

int SP_Session :: getStatus()
{
	return mStatus;
}

int SP_Session :: getRunning()
{
	return mRunning;
}

void SP_Session :: setRunning( int running )
{
	mRunning = running;
}

int SP_Session :: getWriting()
{
	return mWriting;
}

void SP_Session :: setWriting( int writing )
{
	mWriting = writing;
}

int SP_Session :: getReading()
{
	return mReading;
}

void SP_Session :: setReading( int reading )
{
	mReading = reading;
}

SP_IOChannel * SP_Session :: getIOChannel()
{
	return mIOChannel;
}

void SP_Session :: setIOChannel( SP_IOChannel * ioChannel )
{
	mIOChannel = ioChannel;
}

