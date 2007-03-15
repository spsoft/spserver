/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include "spbuffer.hpp"

#include "config.h"
#include "event.h"

SP_Buffer :: SP_Buffer()
{
	mBuffer = evbuffer_new();
}

SP_Buffer :: ~SP_Buffer()
{
	evbuffer_free( mBuffer );
	mBuffer = NULL;
}

int SP_Buffer :: append( const void * buffer, int len )
{
	len = len <= 0 ? strlen( (char*)buffer ) : len;

	return evbuffer_add( mBuffer, (void*)buffer, len );
}

int SP_Buffer :: append( const SP_Buffer * buffer )
{
	return append( buffer->getBuffer(), buffer->getSize() );
}

void SP_Buffer :: erase( int len )
{
	evbuffer_drain( mBuffer, len );
}

void SP_Buffer :: reset()
{
	erase( getSize() );
}

int SP_Buffer :: write( int fd )
{
	return evbuffer_write( mBuffer, fd );
}

int SP_Buffer :: read( int fd )
{
	return evbuffer_read( mBuffer, fd, -1 );
}

const void * SP_Buffer :: getBuffer() const
{
	((char*)(EVBUFFER_DATA( mBuffer )))[ getSize() ] = '\0';
	return EVBUFFER_DATA( mBuffer );
}

int SP_Buffer :: getSize() const
{
	return EVBUFFER_LENGTH( mBuffer );
}

char * SP_Buffer :: getLine()
{
	return evbuffer_readline( mBuffer );
}

int SP_Buffer :: take( char * buffer, int len )
{
	len = evbuffer_remove( mBuffer, buffer, len - 1);
	buffer[ len ] = '\0';

	return len;
}

const void * SP_Buffer :: find( const void * key, size_t len )
{
	//return (void*)evbuffer_find( mBuffer, (u_char*)key, len );

	struct evbuffer * buffer = mBuffer;
	u_char * what = (u_char*)key;

	size_t remain = buffer->off;
	u_char *search = buffer->buffer;
	u_char *p;

	while (remain >= len) {
		if ((p = (u_char*)memchr(search, *what, (remain - len) + 1)) == NULL)
			break;

		if (memcmp(p, what, len) == 0)
			return (p);

		search = p + 1;
		remain = buffer->off - (size_t)(search - buffer->buffer);
	}

	return (NULL);
}

