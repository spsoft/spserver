/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <string.h>

#include "spmsgdecoder.hpp"

#include "spbuffer.hpp"
#include "sputils.hpp"

//-------------------------------------------------------------------

SP_MsgDecoder :: ~SP_MsgDecoder()
{
}

//-------------------------------------------------------------------

SP_DefaultMsgDecoder :: SP_DefaultMsgDecoder()
{
	mBuffer = new SP_Buffer();
}

SP_DefaultMsgDecoder :: ~SP_DefaultMsgDecoder()
{
	if( NULL != mBuffer ) delete mBuffer;
	mBuffer = NULL;
}

int SP_DefaultMsgDecoder :: decode( SP_Buffer * inBuffer )
{
	mBuffer->reset();

	mBuffer->append( inBuffer );

	inBuffer->reset();

	return eOK;
}

SP_Buffer * SP_DefaultMsgDecoder :: getMsg()
{
	return mBuffer;
}

//-------------------------------------------------------------------

SP_LineMsgDecoder :: SP_LineMsgDecoder()
{
	mLine = NULL;
}

SP_LineMsgDecoder :: ~SP_LineMsgDecoder()
{
	if( NULL != mLine ) {
		free( mLine );
		mLine = NULL;
	}
}

int SP_LineMsgDecoder :: decode( SP_Buffer * inBuffer )
{
	if( NULL != mLine ) free( mLine );
	mLine = inBuffer->getLine();

	return NULL == mLine ? eMoreData : eOK;
}

const char * SP_LineMsgDecoder :: getMsg()
{
	return mLine;
}

//-------------------------------------------------------------------

SP_MultiLineMsgDecoder :: SP_MultiLineMsgDecoder()
{
	mQueue = new SP_CircleQueue();
}

SP_MultiLineMsgDecoder :: ~SP_MultiLineMsgDecoder()
{
	for( ; NULL != mQueue->top(); ) {
		free( (void*)mQueue->pop() );
	}

	delete mQueue;
	mQueue = NULL;
}

int SP_MultiLineMsgDecoder :: decode( SP_Buffer * inBuffer )
{
	int ret = eMoreData;

	for( ; ; ) {
		char * line = inBuffer->getLine();
		if( NULL == line ) break;
		mQueue->push( line );
		ret = eOK;
	}

	return ret;
}

SP_CircleQueue * SP_MultiLineMsgDecoder :: getQueue()
{
	return mQueue;
}

//-------------------------------------------------------------------

SP_DotTermMsgDecoder :: SP_DotTermMsgDecoder()
{
	mBuffer = NULL;
}

SP_DotTermMsgDecoder :: ~SP_DotTermMsgDecoder()
{
	if( NULL != mBuffer ) {
		free( mBuffer );
	}
	mBuffer = NULL;
}

int SP_DotTermMsgDecoder :: decode( SP_Buffer * inBuffer )
{
	if( NULL != mBuffer ) {
		free( mBuffer );
		mBuffer = NULL;
	}

	const char * pos = (char*)inBuffer->find( "\r\n.\r\n", 5 );	

	if( NULL == pos ) {
		pos = (char*)inBuffer->find( "\n.\n", 3 );
	}

	if( NULL != pos ) {
		int len = pos - (char*)inBuffer->getBuffer();

		mBuffer = (char*)malloc( len + 1 );
		memcpy( mBuffer, inBuffer->getBuffer(), len );
		mBuffer[ len ] = '\0';

		inBuffer->erase( len );

		/* remove with the "\n.." */
		char * src, * des;
		for( src = des = mBuffer + 1; * src != '\0'; ) {
			if( '.' == *src && '\n' == * ( src - 1 ) ) src++ ;
			* des++ = * src++;
		}
		* des = '\0';

		if( 0 == strcmp( (char*)pos, "\n.\n" ) ) {
			inBuffer->erase( 3 );
		} else  {
			inBuffer->erase( 5 );
		}
		return eOK;
	} else {
		return eMoreData;
	}
}

const char * SP_DotTermMsgDecoder :: getMsg()
{
	return mBuffer;
}

