#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "trace.h"
#include "liblst.h"
#include "sslwrapper.h"
#include "socket.h"
#include "connection.h"
#include "authorization.h"

#define NL	"\n"
#define PIERCLOUD_VERSION "1.0"

typedef struct Buffer {
	char*	buf;
	size_t	len;
} buffer_t;

static char*	getRowFromSocket(Socket_t* soc);
static int	doReadFromServer(Socket_t* soc);
static int	Chunk(const char* chunk, size_t chunk_len, buffer_t* buf, buffer_t* request);
static int	getConnectionRequest(buffer_t* buf, buffer_t* out_buf);
static int	getLocalHostRespond(buffer_t* request, buffer_t* respond);
static char*	getLineBuffer(buffer_t* buf);
static void	cutBuf(buffer_t* buf, size_t len);
static void	increaseBuffer(buffer_t* buf, const char* chunk, size_t chunk_len);

static void printUsage(const char* name)
{
	printf("\npiercloud version: " PIERCLOUD_VERSION "\n\nUsage: %s [id] [login] [password]\n\n", name);
	exit(EXIT_SUCCESS);
}

static void initDaemon(const char* pid_file)
{
	pid_t pid   = 0;
	if (fork())
	{
		exit(EXIT_SUCCESS);
	}
	setsid();
	signal(SIGHUP, SIG_IGN);

	pid = fork();
	if (pid)
	{
		FILE* fp = fopen(pid_file, "w");
		if (!fp)
		{
			exit(EXIT_FAILURE);
		}
		fprintf(fp, "%d\n", pid);
		fclose(fp);
		exit(EXIT_SUCCESS);
	}

	int i;
	chdir("/");
	umask(0);
	for (i = 1; i < 3; i++)
	{
		close(i);
	}
}

char g_id[512]		= {'\0', };
char g_login[512]	= {'\0', };
char g_passwd[512]	= {'\0', };

int main(int argc, char* argv[])
{
	if(argc < 4)
	{
		printUsage(argv[0]);
	}

	initDaemon("/var/run/piercloud.pid");

	strncpy(g_id, argv[1], 512);
	strncpy(g_login, argv[2], 512);
	strncpy(g_passwd, argv[3], 512);

	for (;;)
	{
		int c = Connection("piercloud.com", atoi("443"), NULL);
		if(-1 == c)
		{
			ERROR("Connection()");
			continue;
		}

		SSL_WRAPP(c)

		Socket_t soc;
		if(-1 == InitSocket(&soc, 0, ssl))
		{
			ERROR("InitSocket()");
			goto ssl_cleanup;
		}
		
		if(1 != Authorization(&soc, g_id, "", ""))
		{
			ERROR("Authorisation()");
			FreeSocket(&soc);
			goto ssl_cleanup;
		}

		doReadFromServer(&soc);

		FreeSocket(&soc);

		ssl_cleanup:
		(void)soc;

		SSL_CLEANUP

		TRACE("Connection closed by server");
		if(-1 == CloseConnection(c))
		{
			ERROR("CloseConnection()");
		}
	}
}

static char* getRowFromSocket(Socket_t* soc)
{
	char* row = NULL;
	if(-1 == GetRowFromSocket(&row, soc))
	{
		if(row)
		{
			free(row);
		}
		return NULL;
	}
	return row;
}

static int doReadFromServer(Socket_t* soc)
{
	if(!soc->fn)
	{
		errno = EINVAL;
		return -1;
	}

	static buffer_t buf;
	memset(&buf, 0, sizeof(buffer_t));
	
	static buffer_t request;
	memset(&request, 0, sizeof(buffer_t));

	for(;;)
	{
		char* data = getRowFromSocket(soc);
		if (NULL == data)
		{
			ERROR("getRowFromSocket(soc)");
			break;
		}

		if (-1 == chopStr(data))
		{
			ERROR("chopStr()");
			free(data);
			break;
		}
		
		if (!strlen(data))
		{
			free(data);
			continue;
		}
 
		size_t len = strtol(data, NULL, 16);
		free(data);

		GetBufFromSocket(&data, soc, len);
		if (data == NULL)
		{
			return 0;
		}

		if (*(data) == '0' && len == 1)
		{
			free(data);
			continue;
		}
		
		Chunk(data, len, &buf, &request);
		
		free(data);
	}

	return 0;
}

static int Chunk(const char* chunk, size_t chunk_len, buffer_t* buf, buffer_t* request)
{
	if (chunk == NULL || chunk_len == 0)
	{
		return -1;
	}

	increaseBuffer(buf, chunk, chunk_len);

	int pid	= getConnectionRequest(buf, request);
	if (pid > 0)
	{
		signal(SIGCHLD, SIG_IGN);
		if (!fork())
		{
			buffer_t respond;
			memset(&respond, 0, sizeof(buffer_t));
			// TODO zalepuha
			if (!strstr(request->buf, "<xs:event-subscribe"))
			{
				getLocalHostRespond(request, &respond);
			}
			
			int respond_c = Connection("piercloud.com", atoi("443"), NULL);
			SSL_WRAPP(respond_c)
			Socket_t respond_soc;
			InitSocket(&respond_soc, 0, ssl);
			char* respond_uri = String("%s/%d", g_id, pid); 
			RespondHeader(&respond_soc, respond_uri, "", "", respond.len);
			SSL_write(respond_soc.fn, respond.buf, respond.len); 
			SSL_CLEANUP
			CloseConnection(respond_c);
			exit(0);
		}
		free(request->buf);
		request->buf = NULL;
		request->len = 0;
	}
	
	return 0;
}

static int getConnectionRequest(buffer_t* buf, buffer_t* out_buf)
{
	if (buf == NULL)
	{
		return -1;
	}
	
	static int in_header		= 0;
	static int in_data		= 0;
	static int pid			= 0;
	static size_t content_len	= 0;

	if (!in_header && !in_data)
	{
		in_header = 1;
		content_len = 0;
		char* strpid = getLineBuffer(buf);
		ASSERT(strpid == NULL);	
		pid = atoi(strpid);
		free(strpid);
	}

	for (; in_header; )
	{
		char* header = getLineBuffer(buf);
		ASSERT(header == NULL);	

		char* login_pass_encripted = ptBasicRow(g_login, g_passwd);
		ASSERT(NULL == login_pass_encripted);

		char* host = String("Host: %s.piercloud.com", g_id);
		//YWRtaW46cGFzc3dvcmQ=
		ASSERT(-1 == replaceStrFormat(&header, host, "Host: localhost" NL "Authorization: Basic %s", login_pass_encripted));
		free(host);

		if (header == strcasestr(header, "Content-Length: "))
		{
			 content_len = atoll(header + strlen("Content-Length: "));
		}

		increaseBuffer(out_buf, header, strlen(header));
		increaseBuffer(out_buf, NL, strlen(NL));

		if (!strlen(header))
		{
			in_header = 0;
			if (content_len)
			{
				in_data = 1;
			}
			else
			{
				free(header);
				free(login_pass_encripted);
				return pid;
			}
		}
		free(login_pass_encripted);
		free(header);
	}

	if (in_data)
	{
		if (content_len <= buf->len)
		{
			increaseBuffer(out_buf, buf->buf, content_len);
			cutBuf(buf, content_len);
			in_data = 0;
			return pid;
		}
	}
	
	return 0;
}

static int getLocalHostRespond(buffer_t* request, buffer_t* respond)
{
	int c = Connection("localhost", atoi("80"), NULL);
	if(-1 == c)
	{
		ERROR("Connection()");
		return -1;
	}
	errno = 0;
	Write(c, request->buf, request->len);
	ASSERT(errno);
	//TRACE("\n%d %d > request->buf=%s<END>\n", getpid(), time(NULL), request->buf);

	cutBuf(request, request->len);

	Socket_t s;
	ASSERT(-1 == InitSocket(&s, c, NULL));
	
	char* header = strdup("");
	ASSERT(header == NULL);

	int chunked		= 0;
	size_t content_len	= 0;

	char* data = NULL;
	for (;;)
	{
		data = getRowFromSocket(&s);
		if (NULL == data)
		{
			ERROR("getRowFromSocket(soc)");
			break;
		}

		if (-1 == chopStr(data))
		{
			ERROR("chopStr()");
			free(data);
			break;
		}
					
		if (!strlen(data))
		{
			free(data);
			break;
		}

		if (data == strcasestr(data, "HTTP/1.1 "))
		{
			ASSERT(-1 == replaceStrFormat(&data, "HTTP/1.1 ", "Status: "));
		}

		if (data == strcasestr(data, "Content-Length: "))
		{
			content_len = atoll(data + strlen("Content-Length: "));
		}

		if (data == strcasestr(data, "Transfer-Encoding: chunked"))
		{
			chunked = 1;
			ASSERT(-1 == replaceStrFormat(&header, "", "Content-Length: 0" NL));
			free(data);
			continue;
		}

		ASSERT(-1 == replaceStrFormat(&header, "", "%s" NL, data));
		free(data);
	}

	ASSERT(-1 == replaceStrFormat(&header, "Location: http://localhost/", "Location: http://%s.piercloud.com/", g_id));
	//ASSERT(-1 == replaceStrFormat(&header, "Location: https://192.168.10.144/", "Location: http://1.piercloud.com/"));

	if (!chunked)
	{
		ASSERT(-1 == replaceStrFormat(&header, "",  NL));
		increaseBuffer(respond, header, strlen(header));
		size_t n = GetBufFromSocket(&data, &s, content_len);
		increaseBuffer(respond, data, content_len);
		free(data);
	}
	else
	{
		char* content = NULL;
		size_t chunk_len = 0;
		content_len = 0;
		for (;;)
		{
			data = getRowFromSocket(&s);
			if (NULL == data)
			{
				ERROR("getRowFromSocket(soc)");
				break;
			}
	
			if (-1 == chopStr(data))
			{
				ERROR("chopStr()");
				free(data);
				data = NULL;
				break;
			}
						
			if (!strlen(data))
			{
				free(data);
				data = NULL;
				continue;
			}

			chunk_len = strtol(data, NULL, 16);
			free(data);
			data = NULL;

			if (!chunk_len)
			{
				break;
			}

			GetBufFromSocket(&data, &s, chunk_len);
			char* t = malloc(content_len + chunk_len); 
			ASSERT(t == NULL);
			memcpy(t, content, content_len);
			memcpy(t + content_len, data, chunk_len);
			free(content);
			content = t;
			free(data);
			data = NULL;
			content_len += chunk_len;
		}
		
		ASSERT(-1 == replaceStrFormat(&header, "Content-Length: 0", "Content-Length: %ld", content_len));
		ASSERT(-1 == replaceStrFormat(&header, "", NL));
		increaseBuffer(respond, header, strlen(header));
		increaseBuffer(respond, content, content_len);
		free(content);
	}

	//TRACE("\n%d %d < respond->buf=%s<END>\n", getpid(), time(NULL), respond->buf);
	
	free(header);
	FreeSocket(&s);
	CloseConnection(c);
		
	return 0;
}

static char* getLineBuffer(buffer_t* buf)
{
	char* n = memchr(buf->buf, '\n', buf->len);
	if (n == NULL)
	{
		return NULL;		
	}
	size_t i = n - buf->buf + 1; 
	*n = '\0';
	ASSERT(-1 == chopStr(buf->buf));
	char* rval = strdup(buf->buf);
	cutBuf(buf, i);	

	return rval;
}

static void increaseBuffer(buffer_t* buf, const char* chunk, size_t chunk_len)
{
	char* t = malloc(buf->len + chunk_len + 1); 
	ASSERT(t == NULL);
	memcpy(t, buf->buf, buf->len);
	memcpy(t + buf->len, chunk, chunk_len);
	free(buf->buf);
	buf->buf = t;
	buf->len += chunk_len;
	*(buf->buf + buf->len) = '\0'; 
}

static void cutBuf(buffer_t* buf, size_t len)
{
	buf->len -= len;
	char* t = malloc(buf->len + 1);
	ASSERT(t == NULL);
	memcpy(t, buf->buf + len, buf->len);
	free(buf->buf);
	buf->buf = t;
	*(buf->buf + buf->len) = '\0'; 
}
