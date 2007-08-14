/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */


#ifndef __spbuffer_hpp__
#define __spbuffer_hpp__

#include <stdlib.h>

struct evbuffer;

class SP_Buffer {
public:
	SP_Buffer();
	~SP_Buffer();

	int append( const void * buffer, int len = 0 );
	int append( const SP_Buffer * buffer );
	void erase( int len );
	void reset();
	const void * getBuffer() const;
	size_t getSize() const;
	int take( char * buffer, int len );

	char * getLine();
	const void * find( const void * key, size_t len );

	SP_Buffer * take();

private:
	struct evbuffer * mBuffer;

	friend class SP_IOChannel;
};

#endif

