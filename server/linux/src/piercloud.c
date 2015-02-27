#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>

#include "trace.h"
#include "liblst.h"
#include "network.h"
#include "client.h"

extern char**	environ;
static char**	getEnv();
static int	Pier(long pier);
static int	Connect(long pier);
static void	writeHeaderLst(int Wfd, char** lst);


#define RUN_PATH	"/tmp/piercloud"
#define NL		"\r\n"

main()
{
	void** gc;
	remove(RUN_PATH);
	errno = 0;
	if (-1 == mkdir(RUN_PATH, 00777))
	{
		ASSERT(errno != EEXIST);
		errno = 0;
	}
	
	//printf("Content-type: text/plain\n\n");
	/*
	int i = 0;
	char** lstEnv = getEnv();
	for (; lstEnv[i]; ++i)
	{
		TRACE("%s\n", lstEnv[i]);
	}
	*/

	const char* http_host = getenv("HTTP_HOST");
	ASSERT(http_host == NULL);

	char** lstArgs = lstSplitStr('.', http_host);
	ASSERT(lstArgs == NULL);
	
	int arg_c = lstSize(lstArgs);
	ASSERT(arg_c < 2);
	
	if (arg_c == 2)
	{
		const char* request_uri = getenv("REQUEST_URI");
		ASSERT(request_uri == NULL);
		
		long pier = atol(request_uri + 1);	
		Connect(pier);
	}
	else
	{
		long pier = atol(lstArgs[0]);
		Pier(pier);
	}

	mapFree(lstArgs);
	return 0;
}

static int Open(const char* pathname, int flags, mode_t mode)
{
	int rval = open(pathname, flags, mode);
	ASSERT(-1 == rval);

	return rval;
}

static int MkFifo(const char* path)
{
	ASSERT(path == NULL);

	int safe_errno = errno;
	if(-1 == mkfifo(path, S_IRUSR | S_IWUSR | S_IXUSR )
		&& errno != EEXIST)
	{
		return -1;
	}
	errno = safe_errno;

	return 0;
}

static int Connect(long pier)
{
	ASSERT(!pier);
	void** gc = NULL;
	
	char* Rfifo = String(RUN_PATH "/%ld.in", pier);	
	char* Wfifo = String(RUN_PATH "/%ld.out", pier);	
	ASSERT(Rfifo == NULL || Wfifo == NULL);
	
	gcCollect(&gc, &Wfifo);
	gcCollect(&gc, &Rfifo);
	
	int status = 0;

	status = MkFifo(Rfifo);
	ASSERT(-1 == status);
	
	status = MkFifo(Wfifo);
	ASSERT(-1 == status);
	
	const char* answer = "Status: 200 OK\r\n\r\n";
	Write(1, answer, strlen(answer));

	TRACE("CONNECT> Wait data from %s", Rfifo);
	
	for (;;)
	{
		int Rfd = Open(Rfifo, O_RDONLY, 0);
		int n_read		= 0;
		size_t total		= 0;
		char buf[PIPE_BUF]	= {'\0', };
		errno = 0;
		while((n_read = read(Rfd, buf, sizeof(buf))))
		{
			if(-1 == n_read)
			{
				if(errno == EINTR
					|| errno == EAGAIN
					|| errno == EWOULDBLOCK
					|| errno == EPIPE)
				{
					errno = 0;
					continue;
				}
				close(Rfd);
				break;
			}

			errno = 0;
			Write(1, buf, n_read);
			ASSERT(errno);
			total += n_read;
		}
		close(Rfd);

		int Wfd = Open(Wfifo, O_WRONLY, 0);
		answer = "Status: 200 OK\r\n\r\nOK!\r\n";
		Write(Wfd, answer, strlen(answer));
		close(Wfd); 
	}

	unlink(Rfifo);
	unlink(Wfifo);
	gcClean(gc);
}

static int Pier(long pier)
{
	ASSERT(!pier);
	
	void** gc = NULL;

	char** lstEnv = getEnv();
	ASSERT(lstEnv == NULL);

	char* Wfifo = String(RUN_PATH "/%ld.in", pier);	
	char* Rfifo = String(RUN_PATH "/%ld.out", pier);	
	ASSERT(Rfifo == NULL || Wfifo == NULL);
	
	gcCollect(&gc, &Wfifo);
	gcCollect(&gc, &Rfifo);

	int Wfd = Open(Wfifo, O_WRONLY, 0);
	
	// TODO: replace writeToFileFormat with writeToPipeFormat with max chank = PIPE_BUF
	int header = writeToFileFormat(Wfd, "%s %s %s" NL, 
		getenv("REQUEST_METHOD") ? getenv("REQUEST_METHOD") : "GET", 
		getenv("REQUEST_URI"),
		getenv("SERVER_PROTOCOL") ? getenv("SERVER_PROTOCOL") : "HTTP/1.1");	
	ASSERT(-1 == header);

	char** lstHttpEnv = lstEgrep(lstEnv, "^HTTP_.+$");
	if (lstHttpEnv)
	{
		writeHeaderLst(Wfd, lstHttpEnv);
	}
	
	char** lstContentEnv = lstEgrep(lstEnv, "^CONTENT_.+$");
	if (lstContentEnv)
	{
		writeHeaderLst(Wfd, lstContentEnv);
	}
	
	header = writeToFileFormat(Wfd, "" NL); 
	ASSERT(-1 == header);

	int n_read		= 0;
	size_t total		= 0;
	char buf[PIPE_BUF]	= {'\0', };
	errno = 0;
	while((n_read = read(0, buf, sizeof(buf))))
	{
		if(-1 == n_read)
		{
			if(errno == EINTR
				|| errno == EAGAIN
				|| errno == EWOULDBLOCK
				|| errno == EPIPE)
			{
				errno = 0;
				continue;
			}

			break;
		}

		errno = 0;
		int n = Write(Wfd, buf, n_read);
		ASSERT(errno);
		total += n_read;
	}
	close(Wfd);

	/*
	char* data = NULL;
	gcCollect(&gc, &data);
	errno = 0;
	size_t n_read = Read(0, &data);
	ASSERT(errno);
	//TRACE("PIER> read %ld bytes", n_read);
	errno = 0;
	int n_body = Write(Wfd, data, n_read);
	ASSERT(errno);
	close(Wfd);
	//TRACE("PIER> write %ld bytes", n_body);
	*/

	int Rfd = Open(Rfifo, O_RDONLY, 0);	
	char* data2 = NULL;
	gcCollect(&gc, &data2);
	errno = 0;
	size_t n_read2 = Read(Rfd, &data2);
	ASSERT(errno);
	close(Rfd);
	//TRACE("PIER> read %ld bytes", n_read2);
	errno = 0;
	int n_body2 = Write(1, data2, n_read2);
	ASSERT(errno);
	//TRACE("PIER> write %ld bytes", n_body2);
	
	lstFree(lstEnv);
	lstFree(lstHttpEnv);
	lstFree(lstContentEnv);
	gcClean(gc);
}

static void writeHeaderLst(int Wfd, char** lst)
{
	int i = 0;
	for (; lst[i]; ++i)
	{
		char* p = NULL;
		char* header1 = NULL;
		if (lst[i] == strstr(lst[i], "HTTP_"))
		{
			p = strchr(lst[i], '_');
			if (p == NULL)
			{
				continue;
			}
			*p = '\0';
			header1 = ++p;
		}
		else
		{
			header1 = lst[i];
		}
		p = strchr(header1, '=');
		if (p == NULL)
		{
			continue;
		}
		*p = '\0';
		
		char* header2 = ++p;
		
		p = header1;
		for (++p; *p; ++p) 
		{
			*p = tolower(*p);
			if (*p == '_')
			{
				*p = '-';
			}
		}
		int header = writeToFileFormat(Wfd, "%s: %s" NL, header1, header2); 
		ASSERT(-1 == header);
	}
}

static char** getEnv()
{
	char** lst = NULL;
	char** env = environ;
	for (; *env; ++env)
	{
		lstPush(&lst, *env);
	}
	
	return lst;
}

