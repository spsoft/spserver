/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spiochannel_hpp__
#define __spiochannel_hpp__

class SP_Session;
class SP_Buffer;

struct evbuffer;

class SP_IOChannel {
public:
	virtual ~SP_IOChannel();

	// call by an independence thread, can block
	// return -1 : terminate session, 0 : continue
	virtual int init( int fd ) = 0;

	// run in event-loop thread, cannot block
	// return the number of bytes received, or -1 if an error occurred.
	virtual int receive( SP_Session * session ) = 0;

	// run in event-loop thread, cannot block
	// return the number of bytes sent, or -1 if an error occurred.
	virtual int transmit( SP_Session * session ) = 0;

protected:
	static struct evbuffer * getEvBuffer( SP_Buffer * buffer );
};

class SP_IOChannelFactory {
public:
	virtual ~SP_IOChannelFactory();

	virtual SP_IOChannel * create() const = 0;
};

class SP_DefaultIOChannelFactory : public SP_IOChannelFactory {
public:
	SP_DefaultIOChannelFactory();
	virtual ~SP_DefaultIOChannelFactory();

	virtual SP_IOChannel * create() const;
};

class SP_DefaultIOChannel : public SP_IOChannel {
public:
	SP_DefaultIOChannel();
	~SP_DefaultIOChannel();

	virtual int init( int fd );
	virtual int receive( SP_Session * session );
	virtual int transmit( SP_Session * session );

private:
	int mFd;
};

#endif

