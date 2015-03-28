/*
 *  authorization.h
 *
 *  Author: Andrey Zmushko
 */

#ifndef __AUTHORIZATION_H
#define __AUTHORIZATION_H

#include "socket.h"

char* ptBasicRow(const char* login, const char* pass);
int Authorization(Socket_t* soc, const char* get, const char* login, const char* pass);
int RespondHeader(Socket_t* soc, const char* get, const char* login, const char* pass, size_t length);

#endif
