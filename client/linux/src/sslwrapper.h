/*
 *  sslwrapper.h
 *
 *  Created on: Oct 06, 2012
 *  Author: Andrey Zmushko
 */

#ifndef __SSLWRAPPER_H
#define __SSLWRAPPER_H

#define SSL_WRAPP(A)        OpenSSL_add_all_algorithms();								\
                            SSL_load_error_strings();									\
                            SSL_library_init();										\
															\
                            char err_buf[120] = {'\0'};									\
                            SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());						\
                            if(ctx)											\
                            {												\
				SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);				\
                                SSL* ssl = SSL_new(ctx);								\
                                if(ssl)											\
                                {											\
                                    if(SSL_set_fd(ssl, A) == 1)								\
                                    {											\
                                        if(SSL_connect(ssl) == 1)							\
                                        {										\

                                            /* SSL WRAPPED CODE HERE: c -> ssl */

#define SSL_CLEANUP                         int safe_errno = errno;							\
                                            int i = 0;									\
                                            while(!SSL_shutdown(ssl) && i < 64)						\
                                            {										\
                                                i++;									\
                                            }										\
                                            if(i == 64)									\
                                            {										\
						ERROR("SSL_shutdown");							\
                                            }										\
                                            errno = safe_errno;								\
                                        }										\
                                        else										\
                                        {										\
						char* err_error_string = ERR_error_string(ERR_get_error(), err_buf);    \
						(void)err_error_string;							\
						ERROR("SSL_connect %s",							\
							err_error_string);						\
		                                TRACE("SSL_connect() Error:%s\n", err_error_string);			\
                                        }										\
                                    }											\
                                    else										\
                                    {											\
					char* err_error_string = ERR_error_string(ERR_get_error(), err_buf);		\
					(void)err_error_string;								\
                                        ERROR("SSL_set_fd %s",								\
                                            err_error_string);								\
	                                TRACE("SSL_set_fd() Error:%s\n", err_error_string);				\
                                    }											\
                                SSL_free(ssl);										\
                                }											\
                                else											\
                                {											\
				    char* err_error_string = ERR_error_string(ERR_get_error(), err_buf);		\
			            (void)err_error_string;								\
                                    ERROR("SSL_new: %s",								\
                                        err_error_string);								\
                                    TRACE("SSL_new() Error:%s\n", err_error_string);					\
                                }											\
                            SSL_CTX_free(ctx);										\
                            }												\
                            else											\
                            {												\
				char* err_error_string = ERR_error_string(ERR_get_error(), err_buf);			\
				(void)err_error_string;									\
                                ERROR("SSL_CTX_new: %s",								\
                                    err_error_string);									\
                                TRACE("SSL_CTX_new() Error:%s\n", err_error_string);					\
                            }												\
                            ERR_free_strings();										\
                            EVP_cleanup();										\

#endif
