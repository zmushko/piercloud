/*
 *  base64.c
 *
 *  Created on: Oct 04, 2012
 *  Author: Andrey Zmushko
 */

#include <string.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

char* aBase64Encode(const char* input, const size_t length)
{
	char* buff = NULL;

	BIO* b64 = BIO_new(BIO_f_base64());
	if(b64 == NULL)
	{
		return NULL;
	}
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

	BIO* bmem = BIO_new(BIO_s_mem());
	if(bmem == NULL)
	{
		BIO_free_all(b64);
		return NULL;
	}

	b64 = BIO_push(b64, bmem);
	if(b64 == NULL)
	{
		BIO_free_all(b64);
		return NULL;
	}

	if(BIO_write(b64, input, length) <= 0)
	{
		BIO_free_all(b64);
		return NULL;
	}

	if(BIO_flush(b64) <= 0)
	{
		BIO_free_all(b64);
		return NULL;
	}

	BUF_MEM* bptr = NULL;
	BIO_get_mem_ptr(b64, &bptr);
	if(bptr == NULL)
	{
		BIO_free_all(b64);
		return NULL;
	}

	buff = (char*)malloc(bptr->length + 1);
	if(buff == NULL)
	{
		BIO_free_all(b64);
		return NULL;
	}

	memcpy(buff, bptr->data, bptr->length);
	buff[bptr->length] = 0;

	BIO_free_all(b64);

	return buff;
}
