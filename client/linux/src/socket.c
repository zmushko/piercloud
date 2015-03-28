/*
 *  socket.c
 *
 *  Author: Andrey Zmushko
 */

#include "trace.h"
#include "socket.h"

#define READ_BUF_SIZE 512

/**
 * @brief read and cut/return row from the head of the Socket buffer, and keep tail in the buffer
 * @param[in] soc Socet structure with SSL socket and buffer
 * @return row from the head of the buffer
 */
static char* GetRowAndCut(Socket_t* soc)
{
	if(soc == NULL || soc->buf == NULL)
	{
		ERROR("wrong argument");
		return NULL;
	}

	char* tail = strchr(soc->buf, '\n');
	if(tail == NULL)
	{
		return NULL;
	}
	*tail = '\0';

	char* rval = strdup(soc->buf);
	if(rval == NULL)
	{
		ERROR("malloc()");
		*tail = '\n';
		return NULL;
	}

	size_t new_buf_len = soc->buf_len - strlen(rval) - 1;
	char* t = malloc(new_buf_len + 1);
	if(t == NULL)
	{
		ERROR("malloc()");
		free(rval);
		return NULL;
	}
	memcpy(t, tail + 1, new_buf_len);
	free(soc->buf);
	soc->buf = t;
	soc->buf_len = new_buf_len;
	*(soc->buf + soc->buf_len) = '\0';
	
	return rval;
}

/**
 * @brief try to read size bites from SSL socket and add it in tail of the Socket buffer
 * @param[in] soc Socet structure with SSL socket and buffer
 * @param[in] size to read data
 * @return size of read data
 */
static ssize_t IncreaseSoc(Socket_t* soc, const size_t size)
{
	if (soc == NULL || soc->buf == NULL)
	{
		ERROR("wrong argument");
		return -1;
	}

	char* buf = malloc(size + 1);
	if (buf == NULL)
	{
		ERROR("malloc()");
		return -1;
	}
	memset(buf, 0, size + 1);

	ssize_t r = 0;
        // add rehandshake
        for (;;)
        {
		if (soc->sd > 0)
		{
			errno = 0;
			r = read(soc->sd, buf, size);		
			if(errno == EINTR
				|| errno == EAGAIN
				|| errno == EWOULDBLOCK
				|| errno == EPIPE)
			{
				errno = 0;
				continue;
			}
		}
		else
		{
			r = SSL_read(soc->fn, buf, size);
			if (SSL_ERROR_WANT_READ == SSL_get_error(soc->fn, r))
			{
				continue;
			}
		}
		break;
        }

	if (r == 0)
	{
		//TRACE("CLIENT> SSL_read() return:0 Connection closed by peer");
		free(buf);
		return 0;
	}

	if (r < 0)
	{
		if (soc->sd > 0)
		{
			ERROR("read()");
		}
		else
		{
			char err_buf[256]	= {'\0', };
			ERROR("SSL_read() return:%ld error:%s. Connection closed", (long int)r,
				ERR_error_string(ERR_get_error(), err_buf));
		}
		free(buf);
		return -1;
	}

	size_t new_buf_len = soc->buf_len + r;
	char* t = realloc(soc->buf, new_buf_len + 1);
	if (t == NULL)
	{
		ERROR("realloc()");
		free(buf);
		return -1;
	}
	soc->buf = t;
	memcpy(soc->buf + soc->buf_len, buf, r);
	free(buf);
	soc->buf_len = new_buf_len;
	*(soc->buf + soc->buf_len) = '\0';
	//TDATA("%s", soc->buf);
	
	return r;
}

/**
 * @brief read row from SSL socket
 * @param[in] row pointer to read data
 * @param[in] soc Socet structure with SSL socket and buffer
 * @return size of read data
 */
ssize_t GetRowFromSocket(char** row, Socket_t* soc)
{
	if (soc == NULL)
	{
		ERROR("soc == NULL");
		*row = NULL;
		return -1;
	} 

	if (soc->buf == NULL)
	{
		ERROR("soc->buf == NULL");
		*row = NULL;
		return -1;
	}

	ssize_t r = 0;
	while(NULL == (*row = GetRowAndCut(soc)))
	{
		r = IncreaseSoc(soc, READ_BUF_SIZE);
		if(r <= 0)
		{
			free(soc->buf);
			soc->buf = NULL;
			soc->buf_len = 0;
			return -1;
		}
	}

	return r;
}

/**
 * @brief read buffer with size length from SSL socket
 * @param[in] buf pointer to read data
 * @param[in] soc Socet structure with SSL socket and buffer
 * @param[in] size to read buffer
 * @return size of read data
 */
ssize_t GetBufFromSocket(char** buf, Socket_t* soc, ssize_t size)
{
	if (soc == NULL || size < 0 || soc->buf == NULL)
	{
		ERROR("wrong argument");
		*buf = NULL;
		return -1;
	}

	if (size == 0)
	{
		*buf = NULL;
		return 0;
	}

	ssize_t r = 0;
	while ((r = soc->buf_len) < size)
	{
		r = IncreaseSoc(soc, size - r);
		if (r <= 0)
		{
			free(soc->buf);
			soc->buf = NULL;
			soc->buf_len = 0;
			return r;
		}
	}

	*buf = malloc(soc->buf_len + 1);
	if (*buf == NULL)
	{
		ERROR("malloc()");
		free(soc->buf);
		soc->buf = NULL;
		soc->buf_len = 0;
		return -1;
	}
	memcpy(*buf, soc->buf, soc->buf_len);
	free(soc->buf);
	soc->buf = strdup("");
	if (soc->buf == NULL)
	{
		ERROR("malloc()");
		free(soc->buf);
		soc->buf = NULL;
		soc->buf_len = 0;
		return -1;
	}
	soc->buf_len = 0;
	*(*buf + size) = '\0';

	return r;
}

/**
 * @brief initial Socet structure
 * @param[in] soc Socet structure
 * @param[in] ssl socket
 * @return size of read data
 */
int InitSocket(Socket_t* soc, int sd, SSL* ssl)
{
	if (sd > 0)
	{
		soc->sd = sd;
		soc->fn = NULL;
	}
	else if (ssl != NULL)
	{
		soc->sd = 0;
		soc->fn = ssl;
	}
	else
	{
		errno = EINVAL;
		ERROR("Invalid argument");
		return -1;
	}

	soc->buf_len = 0;
	soc->buf = strdup("");
	if (soc->buf == NULL)
	{
		ERROR("malloc()");
		return -1;
	}
	
	return 0;
}

/**
 * @brief release Socet structure
 * @param[in] soc Socet structure
 * @return size of read data
 */
void FreeSocket(Socket_t* soc)
{
	soc->fn = NULL;
	free(soc->buf);
	soc->buf = NULL;
	soc->buf_len = 0;
}
