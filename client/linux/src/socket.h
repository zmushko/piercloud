/*
 *  socket.h
 *
 *  Created on: May 2013
 *  Author: Andrey Zmushko
 */

#ifndef __SOCKET_H
#define __SOCKET_H

#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct {
	int	sd;
	SSL*	fn;
	char*	buf;
	size_t	buf_len;
} Socket_t;


int InitSocket(Socket_t* soc, int sd, SSL* ssl);
ssize_t GetRowFromSocket(char** row, Socket_t* soc);
ssize_t GetBufFromSocket(char** buf, Socket_t* soc, const ssize_t size);
void FreeSocket(Socket_t* soc);

#endif
