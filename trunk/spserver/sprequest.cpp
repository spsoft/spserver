/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <string.h>
#include <stdio.h>

#include "spporting.hpp"

#include "sprequest.hpp"
#include "spmsgdecoder.hpp"

SP_Request :: SP_Request()
{
	memset( mClientIP, 0, sizeof( mClientIP ) );
	mDecoder = new SP_DefaultMsgDecoder();
}

SP_Request :: ~SP_Request()
{
	if( NULL != mDecoder ) delete mDecoder;
	mDecoder = NULL;
}

SP_MsgDecoder * SP_Request :: getMsgDecoder()
{
	return mDecoder;
}

void SP_Request :: setMsgDecoder( SP_MsgDecoder * decoder )
{
	if( NULL != mDecoder ) delete mDecoder;
	mDecoder = decoder;
}

void SP_Request :: setClientIP( const char * clientIP )
{
	snprintf( mClientIP, sizeof( mClientIP ), "%s", clientIP );
}

const char * SP_Request :: getClientIP()
{
	return mClientIP;
}

