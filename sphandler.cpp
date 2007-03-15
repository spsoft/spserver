/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include "sphandler.hpp"
#include "spresponse.hpp"

SP_Handler :: ~SP_Handler()
{
}

//---------------------------------------------------------

SP_CompletionHandler :: ~SP_CompletionHandler()
{
}

//---------------------------------------------------------

class SP_DefaultCompletionHandler : public SP_CompletionHandler {
public:
	SP_DefaultCompletionHandler(){}
	~SP_DefaultCompletionHandler(){}

	virtual void completionMessage( SP_Message * msg ) { delete msg; }
};

//---------------------------------------------------------

SP_HandlerFactory :: ~SP_HandlerFactory()
{
}

SP_CompletionHandler * SP_HandlerFactory :: createCompletionHandler() const
{
	return new SP_DefaultCompletionHandler();
}

