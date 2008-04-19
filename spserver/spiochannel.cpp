/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <string.h>
#include <assert.h>

#include "spporting.hpp"

#include "spiochannel.hpp"

#include "speventcb.hpp"
#include "sputils.hpp"
#include "spresponse.hpp"
#include "spsession.hpp"
#include "spbuffer.hpp"
#include "spmsgblock.hpp"

#include "event.h"

//---------------------------------------------------------

SP_IOChannel :: ~SP_IOChannel()
{
}

struct evbuffer * SP_IOChannel :: getEvBuffer( SP_Buffer * buffer )
{
	return buffer->mBuffer;
}

int SP_IOChannel :: transmit( SP_Session * session )
{
#ifdef IOV_MAX
	const static int SP_MAX_IOV = IOV_MAX;
#else
	const static int SP_MAX_IOV = 8;
#endif

	SP_EventArg * eventArg = (SP_EventArg*)session->getArg();

	SP_ArrayList * outList = session->getOutList();
	size_t outOffset = session->getOutOffset();

	struct iovec iovArray[ SP_MAX_IOV ];
	memset( iovArray, 0, sizeof( iovArray ) );

	int iovSize = 0;

	for( int i = 0; i < outList->getCount() && iovSize < SP_MAX_IOV; i++ ) {
		SP_Message * msg = (SP_Message*)outList->getItem( i );

		if( outOffset >= msg->getMsg()->getSize() ) {
			outOffset -= msg->getMsg()->getSize();
		} else {
			iovArray[ iovSize ].iov_base = (char*)msg->getMsg()->getBuffer() + outOffset;
			iovArray[ iovSize++ ].iov_len = msg->getMsg()->getSize() - outOffset;
			outOffset = 0;
		}

		SP_MsgBlockList * blockList = msg->getFollowBlockList();
		for( int j = 0; j < blockList->getCount() && iovSize < SP_MAX_IOV; j++ ) {
			SP_MsgBlock * block = (SP_MsgBlock*)blockList->getItem( j );

			if( outOffset >= block->getSize() ) {
				outOffset -= block->getSize();
			} else {
				iovArray[ iovSize ].iov_base = (char*)block->getData() + outOffset;
				iovArray[ iovSize++ ].iov_len = block->getSize() - outOffset;
				outOffset = 0;
			}
		}
	}

	int len = write_vec( iovArray, iovSize );

	if( len > 0 ) {
		outOffset = session->getOutOffset() + len;

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
	}

	if( len > 0 && outList->getCount() > 0 ) transmit( session );

	return len;
}

//---------------------------------------------------------

SP_IOChannelFactory :: ~SP_IOChannelFactory()
{
}

//---------------------------------------------------------

SP_DefaultIOChannel :: SP_DefaultIOChannel()
{
	mFd = -1;
}

SP_DefaultIOChannel :: ~SP_DefaultIOChannel()
{
	mFd = -1;
}

int SP_DefaultIOChannel :: init( int fd )
{
	mFd = fd;

	return 0;
}

int SP_DefaultIOChannel :: receive( SP_Session * session )
{
	return evbuffer_read( getEvBuffer( session->getInBuffer() ), mFd, -1 );
}

int SP_DefaultIOChannel :: write_vec( struct iovec * iovArray, int iovSize )
{
	return sp_writev( mFd, iovArray, iovSize );
}

//---------------------------------------------------------

SP_DefaultIOChannelFactory :: SP_DefaultIOChannelFactory()
{
}

SP_DefaultIOChannelFactory :: ~SP_DefaultIOChannelFactory()
{
}

SP_IOChannel * SP_DefaultIOChannelFactory :: create() const
{
	return new SP_DefaultIOChannel();
}

