/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __sputils_hpp__
#define __sputils_hpp__

class SP_ArrayList {
public:
	static const int LAST_INDEX;

	SP_ArrayList( int initCount = 2 );
	virtual ~SP_ArrayList();

	int getCount() const;
	int append( void * value );
	const void * getItem( int index ) const;
	void * takeItem( int index );

private:
	SP_ArrayList( SP_ArrayList & );
	SP_ArrayList & operator=( SP_ArrayList & );

	int mMaxCount;
	int mCount;
	void ** mFirst;
};

#endif

