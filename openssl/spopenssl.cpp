/*
 * Copyright 2007 Stephen Liu
 * For license terms, see the file COPYING along with this library.
 */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <openssl/rsa.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include "spopenssl.hpp"
#include "spsession.hpp"
#include "spbuffer.hpp"
#include "speventcb.hpp"
#include "sputils.hpp"
#include "spmsgblock.hpp"
#include "spioutils.hpp"

SP_OpensslChannel :: SP_OpensslChannel( SSL_CTX * ctx )
{
	mCtx = ctx;
	mSsl = NULL;
}

SP_OpensslChannel :: ~SP_OpensslChannel()
{
	if( NULL != mSsl ) SSL_free( mSsl );
	mSsl = NULL;
}

int SP_OpensslChannel :: init( int fd )
{
	char errmsg[ 256 ] = { 0 };

	mSsl = SSL_new( mCtx );
	SSL_set_fd( mSsl, fd );

	/* we run in an independence thread, and we can block when SSL_accept */

	SP_IOUtils::setBlock( fd );
	int ret = SSL_accept( mSsl );
	if( ret <= 0 ) {
		ERR_error_string_n( SSL_get_error( mSsl, ret ), errmsg, sizeof( errmsg ) );
		syslog( LOG_EMERG, "SSL_accept fail, %s", errmsg );
		return -1;
	}

	SP_IOUtils::setNonblock( fd );

	/* Get the cipher - opt */

	syslog( LOG_NOTICE, "SSL connection using %s", SSL_get_cipher( mSsl ) );
  
	/* Get client's certificate (note: beware of dynamic allocation) - opt */

	X509 * client_cert = SSL_get_peer_certificate( mSsl );
	if( client_cert != NULL ) {
		syslog( LOG_NOTICE, "Client certificate:" );
 
		char * str = X509_NAME_oneline( X509_get_subject_name( client_cert ), 0, 0 );
		if( NULL != str ) {
			syslog( LOG_NOTICE, "subject: %s", str );
			OPENSSL_free( str );
		}

		str = X509_NAME_oneline( X509_get_issuer_name( client_cert ), 0, 0 );
		if( NULL != str ) {
			syslog( LOG_NOTICE, "issuer: %s", str );
			OPENSSL_free( str );
		}

		/* We could do all sorts of certificate verification stuff here before
		   deallocating the certificate. */

		X509_free( client_cert );
	} else {
		syslog( LOG_WARNING, "Client does not have certificate" );
	}

	return 0;
}

int SP_OpensslChannel :: receive( SP_Session * session )
{
	char buffer[ 4096 ] = { 0 };

	int ret = SSL_read( mSsl, buffer, sizeof( buffer ) );
	if( ret > 0 ) {
		session->getInBuffer()->append( buffer, ret );
	} else if( ret < 0 ) {
		ERR_error_string_n( ERR_get_error(), buffer, sizeof( buffer ) );
		syslog( LOG_EMERG, "SSL_read fail, %s", buffer );
	}

	return ret;
}

int SP_OpensslChannel :: transmit( SP_Session * session )
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

	int len = 0;

	for( int i = 0; i < iovSize; i++ ) {
		int ret = SSL_write( mSsl, iovArray[i].iov_base, iovArray[i].iov_len );
		if( ret > 0 ) len += ret;
		if( ret != (int)iovArray[i].iov_len ) break;
	}

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

SP_OpensslChannelFactory :: SP_OpensslChannelFactory()
{
	mCtx = NULL;
}

SP_OpensslChannelFactory :: ~SP_OpensslChannelFactory()
{
	if( NULL != mCtx ) SSL_CTX_free( mCtx );
	mCtx = NULL;
}

SP_IOChannel * SP_OpensslChannelFactory :: create() const
{
	return new SP_OpensslChannel( mCtx );
}

int SP_OpensslChannelFactory :: init( const char * certFile, const char * keyFile )
{
	unsigned char strRand[ 32 ] ;
	memset( strRand, 0, sizeof( strRand ) );
	snprintf( (char*)strRand, sizeof( strRand ), "%d%ld", getpid(), time(NULL) );
	RAND_seed( strRand, sizeof( strRand ) );

	RAND_load_file( "/dev/urandom", 256 );

	int ret = 0;
	char errmsg[ 256 ] = { 0 };

	ERR_load_crypto_strings ();
	SSL_load_error_strings();
	SSLeay_add_ssl_algorithms();

	mCtx = SSL_CTX_new( SSLv23_server_method() );
	if( ! mCtx ) {
		ERR_error_string_n( ERR_get_error(), errmsg, sizeof( errmsg ) );
		syslog( LOG_WARNING, "SSL_CTX_new fail, %s", errmsg );
		ret = -1;
	}

	if( 0 == ret ) {
		if( SSL_CTX_use_certificate_file( mCtx, certFile, SSL_FILETYPE_PEM ) <= 0 ) {
			ERR_error_string_n( ERR_get_error(), errmsg, sizeof( errmsg ) );
			syslog( LOG_WARNING, "SSL_CTX_use_certificate_file fail, %s", errmsg );
			ret = -1;
		}
	}

	if( 0 == ret ) {
		if( SSL_CTX_use_PrivateKey_file( mCtx, keyFile, SSL_FILETYPE_PEM ) <= 0 ) {
			ERR_error_string_n( ERR_get_error(), errmsg, sizeof( errmsg ) );
			syslog( LOG_WARNING, "SSL_CTX_use_PrivateKey_file fail, %s", errmsg );
			ret = -1;
		}
	}

	if( 0 == ret ) {
		if( !SSL_CTX_check_private_key( mCtx ) ) {
			ERR_error_string_n( ERR_get_error(), errmsg, sizeof( errmsg ) );
			syslog( LOG_WARNING, "Private key does not match the certificate public key, %s", errmsg );
			ret = -1;
		}
	}

	return ret;
}

