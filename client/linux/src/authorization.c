/*
 *  authorization.c
 *
 *  Author: Andrey Zmushko
 */
#include <string.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "trace.h"
#include "liblst.h"
#include "base64/base64.h"
#include "socket.h"

#define TMPL_BASE64_LOGIN_PASS	"%s:%s"

//#define TMPL_AUTH_HEADER	"GET /%s HTTP/1.1\r\n"		\
//				"Authorization: Basic %s\r\n"   \
//				"Host: readycloud\r\n"		\
//				"Accept: */*\r\n"		\
//				"Content-Length: 0\r\n"		\
//				"\r\n"

#define TMPL_AUTH_HEADER	"GET /%s HTTP/1.1\r\n"		\
				"Host: piercloud.com\r\n"	\
				"\r\n"

#define TMPL_AUTH_HEADER2	"PUT /%s HTTP/1.1\r\n"		\
				"Host: piercloud.com\r\n"	\
				"Content-Length: %ld\r\n"	\
				"\r\n"

char* ptBasicRow(const char* login, const char* pass)
{
	char* login_pass = malloc(snprintf(NULL, 0, TMPL_BASE64_LOGIN_PASS, login, pass) + 1);
	if(NULL == login_pass)
	{
		ERROR("malloc()");
		return NULL;
	}
	sprintf(login_pass, TMPL_BASE64_LOGIN_PASS, login, pass);

	char* login_pass_encripted = aBase64Encode(login_pass, strlen(login_pass));
	if(NULL == login_pass_encripted)
	{
		ERROR("aBase64Encode(%s)", login_pass);
		free(login_pass);
		return NULL;
	}

	free(login_pass);
	return login_pass_encripted;
}

int Authorization(Socket_t* soc, const char* get, const char* login, const char* pass)
{
	TRACE("CLIENT>Authorisation...");

	char* login_pass_encripted = ptBasicRow(login, pass);
	if(NULL == login_pass_encripted)
	{
		ERROR("aBase64Encode(%s %s)", login, pass);
		return -1;
	}

	char* Header2 = malloc(snprintf(NULL, 0, TMPL_AUTH_HEADER, get/*, login_pass_encripted*/) + 1);
	if(NULL == Header2)
	{
		ERROR("malloc()");
		free(login_pass_encripted);
		return -1;
	}
	sprintf(Header2, TMPL_AUTH_HEADER, get/*, login_pass_encripted*/);

	int nw = SSL_write(soc->fn, Header2, strlen(Header2));
	if(nw <= 0)
	{
		char err_buf[256]	= {'\0', };
		TRACE("***ERROR*** SSL_write() return:%d error:%s\n", nw,
			ERR_error_string(ERR_get_error(), err_buf));

		/*
		char err_buf[120] = {'\0'};
		char* err_error_string = ERR_error_string(ERR_get_error(), err_buf);
		(void)err_error_string;
		ERROR("ERROR:%s:%d:SSL %s\n",  __FILE__, __LINE__,
			err_error_string);
		*/
		free(Header2);
		free(login_pass_encripted);
		return -1;
	}
	//TRACE("CLIENT>SSL_write() %d bites", nw);
	//TDATA("  REQUEST>\n%s\n", Header2);

	size_t content_len	= 0;
	char* buf		= NULL;
	int i			= 0;
	int rval		= -1;
	while (-1 != GetRowFromSocket(&buf, soc))
	{
		i++;
		if (-1 == chopStr(buf))
		{
			free(buf);
			buf = NULL;
			break;
		}

		if (0 == strlen(buf))
		{
			free(buf);
			buf = NULL;
			break;
		}

		if (strcasestr(buf, "Content-Length: "))
		{
			 content_len = atoll(buf + strlen("Content-Length: "));
		}

		if (i == 1)
		{
			char** lst = lstSplitStr(' ', buf);
			if (lst)
			{
				if (lstSize(lst) == 3
					&& strcasestr(lst[1], "200")
					&& strcasestr(lst[2], "OK"))
				{
					rval = 1;
				}
				else
				{
					rval = 0;
				}
				lstFree(lst);
			}
		}
		free(buf);
		buf = NULL;
	}

	free(buf);
	buf = NULL;

	if (!i)
	{
		ERROR("GetRowFromSocket()");
		free(Header2);
		free(login_pass_encripted);
		TRACE("CLIENT>Authorisation failed");
		return -1;
	}

	if (content_len)
	{
		GetBufFromSocket(&buf, soc, content_len);
		if (!buf)
		{
			ERROR("GetBufFromSocket()");
			free(Header2);
			free(login_pass_encripted);
			TRACE("CLIENT>Authorisation %s", (rval == 1) ? "success" : "fail");
			return rval;
		}
		free(buf);
	}

	free(Header2);
	free(login_pass_encripted);

	TRACE("CLIENT>Authorisation %s", (rval == 1) ? "success" : "fail");

	return rval;
}

int RespondHeader(Socket_t* soc, const char* get, const char* login, const char* pass, size_t length)
{
	char* login_pass_encripted = ptBasicRow(login, pass);
	if(NULL == login_pass_encripted)
	{
		ERROR("aBase64Encode(%s %s)", login, pass);
		return -1;
	}
	
	char* Header2 = malloc(snprintf(NULL, 0, TMPL_AUTH_HEADER2, get/*, login_pass_encripted*/, length) + 1);
	//char* Header2 = malloc(snprintf(NULL, 0, TMPL_AUTH_HEADER, get/*, login_pass_encripted*/) + 1);
	if(NULL == Header2)
	{
		ERROR("malloc()");
		free(login_pass_encripted);
		return -1;
	}
	sprintf(Header2, TMPL_AUTH_HEADER2, get/*, login_pass_encripted*/, length);
	//sprintf(Header2, TMPL_AUTH_HEADER, get/*, login_pass_encripted*/);

	int nw = SSL_write(soc->fn, Header2, strlen(Header2));
	if(nw <= 0)
	{
		char err_buf[256]	= {'\0', };
		TRACE("***ERROR*** SSL_write() return:%d error:%s\n", nw,
			ERR_error_string(ERR_get_error(), err_buf));
		free(Header2);
		free(login_pass_encripted);
		return -1;
	}
	//TRACE("CLIENT>SSL_write() %d bites", nw);
	//TRACE("RR>\n%s<END>\n", Header2);

	free(Header2);
	free(login_pass_encripted);

	return 0;
}
