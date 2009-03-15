/*
 * Copyright 2009 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#ifndef __spsmtp_hpp__
#define __spsmtp_hpp__

#include "sphandler.hpp"

class SP_Buffer;

class SP_SmtpHandler {
public:
	virtual ~SP_SmtpHandler();

	virtual void error();

	virtual void timeout();

	// @return 0 : OK, -1 : Fail
	virtual int welcome( SP_Buffer * reply );

	// @return 0 : OK, -1 : Fail
	virtual int helo( const char * args, SP_Buffer * reply );

	// @return 0 : OK, -1 : Fail
	virtual int noop( const char * args, SP_Buffer * reply );

	/**
	 * Called first, after the MAIL FROM during a SMTP exchange.
	 *
	 * @param args is the args of the MAIL FROM
	 *
	 * @return 0 : OK, -1 : Fail
	 */
	virtual int from( const char * args, SP_Buffer * reply ) = 0;

	/**
	 * Called once for every RCPT TO during a SMTP exchange.
	 * This will occur after a from() call.
	 *
	 * @param args is the args of the RCPT TO
	 *
	 * @return 0 : OK, -1 : Fail
	 */
	virtual int rcpt( const char * args, SP_Buffer * reply ) = 0;

	/**
	 * Called when the DATA part of the SMTP exchange begins.  Will
	 * only be called if at least one recipient was accepted.
	 *
	 * @param data will be the smtp data stream, stripped of any extra '.' chars
	 *
	 * @return 0 : OK, -1 : Fail
	 */
	virtual int data( const char * data, SP_Buffer * reply ) = 0;
};

class SP_SmtpHandlerFactory {
public:
	virtual ~SP_SmtpHandlerFactory();

	virtual SP_SmtpHandler * create() const = 0;
};

class SP_SmtpHandlerAdapterFactory : public SP_HandlerFactory {
public:
	SP_SmtpHandlerAdapterFactory( SP_SmtpHandlerFactory * factory );

	virtual ~SP_SmtpHandlerAdapterFactory();

	virtual SP_Handler * create() const;

private:
	SP_SmtpHandlerFactory * mFactory;
};

#endif

