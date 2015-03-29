#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <sys/select.h>

#include "trace.h"
#include "liblst.h"

extern char**	environ;
static char**	getEnv();
static int	Pier(long pier);
static int	Connect(long pier);
static int	Respond(long pier, long pid);
static void	addHeader(char** header, char** lst);


#define RUN_PATH	"/tmp/piercloud"
#define NL		"\r\n"
#define SEC_TO_PING	20

#define STDIN		0
#define STDOUT		1

main()
{
	void** gc;
	errno = 0;
	if (-1 == mkdir(RUN_PATH, 00700))
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
		
		char* t = strchr(request_uri + 1, '/');
		if (t == NULL)
		{
			Connect(atol(request_uri + 1));
		}
		else
		{
			*t = '\0';
			Respond(atol(request_uri + 1), atol(t + 1));
		}
	}
	else
	{
		Pier(atol(lstArgs[0]));
	}

	mapFree(lstArgs);
	return 0;
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

static int Respond(long pier, long pid)
{
	ASSERT(!pier || !pid);	
	void** gc = NULL;
	
	char* Wfifo = String(RUN_PATH "/%ld.0/%ld", pier, pid);	
	ASSERT(Wfifo == NULL);
	gcCollect(&gc, &Wfifo);

	int Wfd = open(Wfifo, O_WRONLY, 0);
	ASSERT(-1 == Wfd);
	
	int n_read		= 0;
	size_t total		= 0;
	char buf[PIPE_BUF]	= {'\0', };
	errno = 0;
	while((n_read = read(STDIN, buf, sizeof(buf))))
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
	
	char* answer = "Status: 200 OK" NL NL;
	Write(STDOUT, answer, strlen(answer));
	
	gcClean(gc);

	return 0;
}

static int Connect(long connect)
{
	ASSERT(!connect);
	void** gc = NULL;
	
	char* Rfifo = String(RUN_PATH "/%ld", connect);	
	ASSERT(Rfifo == NULL);
	gcCollect(&gc, &Rfifo);

	char* lock_name = String(RUN_PATH "/%ld.lock", connect);	
	ASSERT(lock_name == NULL);
	gcCollect(&gc, &lock_name);

	ASSERT(-1 == creat(lock_name, 00600));	
	ASSERT(-1 == MkFifo(Rfifo));
	
	char* answer = String("Status: 200 OK" NL NL);
	ASSERT(answer == NULL);
	gcCollect(&gc, &answer);
	Write(1, answer, strlen(answer));

	for (;;)
	{
		int Rfd = open(Rfifo, O_RDONLY | O_NONBLOCK, 0);		
		ASSERT(-1 == Rfd);
	
		fd_set set;
		FD_ZERO(&set);
		FD_SET(Rfd, &set);
		struct timeval t = { SEC_TO_PING, 0 };
		int rd = select(Rfd + 1, &set, NULL, NULL, &t);
		ASSERT(-1 == rd);
		FD_CLR(Rfd, &set);
		if (!rd)
		{
			errno = 0;
			Write(STDOUT, "0", 1);
			ASSERT(errno);
			close(Rfd);
			continue;
		}
		
		int n_read		= 0;
		size_t total		= 0;
		char buf[PIPE_BUF]	= {'\0', };
		errno = 0;
		while((n_read = read(Rfd, buf, sizeof(buf) - 1)))
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
			int n = Write(STDOUT, buf, n_read);
			ASSERT(errno);
			total += n;
		}
		close(Rfd);
	}

	unlink(Rfifo);
	unlink(lock_name);
	gcClean(gc);
}

static int Pier(long connect)
{
	ASSERT(!connect);	
	void** gc = NULL;
	pid_t my_pid = getpid();

	char** lstEnv = getEnv();
	ASSERT(lstEnv == NULL);

	char* Rpath = String(RUN_PATH "/%ld.0", connect);	
	ASSERT(Rpath == NULL);
	gcCollect(&gc, &Rpath);
	if (-1 == mkdir(Rpath, 00700))
	{
		ASSERT(errno != EEXIST);
		errno = 0;
	}

	char* Rfifo = String(RUN_PATH "/%ld.0/%ld", connect, my_pid);	
	ASSERT(Rfifo == NULL);
	gcCollect(&gc, &Rfifo);
	
	ASSERT(-1 == MkFifo(Rfifo));		

	char* Wfifo = String(RUN_PATH "/%ld", connect);	
	ASSERT(Wfifo == NULL);
	gcCollect(&gc, &Wfifo);

	char* lock_name = String(RUN_PATH "/%ld.lock", connect);	
	ASSERT(lock_name == NULL);
	gcCollect(&gc, &lock_name);

	int Lfd = open(lock_name, O_RDWR);
	if (-1 == Lfd)
	{
		char* answer = String("Status: 434 Requested host unavailable" NL NL \
			"434 Requested host unavailable.");
		gcCollect(&gc, &answer);
		Write(STDOUT, answer, strlen(answer));
		lstFree(lstEnv);
		gcClean(gc);
		unlink(Rfifo);

		return 0;
	}

	if(-1 == lockf(Lfd, F_LOCK, 0L))
	{
		char* answer = String("Status: 434 Requested host unavailable" NL NL \
			"434 Requested host unavailable.");
		gcCollect(&gc, &answer);
		Write(STDOUT, answer, strlen(answer));
		lstFree(lstEnv);
		gcClean(gc);
		unlink(Rfifo);

		return 0;
	}

	int Wfd = open(Wfifo, O_WRONLY | O_SYNC, 0);
	if (-1 == Wfd)
	{
		char* answer = String("Status: 434 Requested host unavailable" NL NL \
			"434 Requested host unavailable.");
		gcCollect(&gc, &answer);
		Write(STDOUT, answer, strlen(answer));
		lstFree(lstEnv);
		gcClean(gc);
		unlink(Rfifo);

		return 0;
	}

	/*
	int i = 0;
	char** lstEnv2 = getEnv();
	for (; lstEnv2[i]; ++i)
	{
		TRACE("%s\n", lstEnv2[i]);
	}
	*/
	
	char* header = String("%d" NL, my_pid);	
	ASSERT(header == NULL);
	gcCollect(&gc, &header);

	ASSERT(-1 == replaceStrFormat(&header, "", "%s %s %s" NL, 
		getenv("REQUEST_METHOD") ? getenv("REQUEST_METHOD") : "GET", 
		getenv("REQUEST_URI"),
		getenv("SERVER_PROTOCOL") ? getenv("SERVER_PROTOCOL") : "HTTP/1.1"));

	char** lstHttpEnv = lstEgrep(lstEnv, "^HTTP_.+$");
	if (lstHttpEnv)
	{
		addHeader(&header, lstHttpEnv);
	}
	
	char** lstContentEnv = lstEgrep(lstEnv, "^CONTENT_.+$");
	if (lstContentEnv)
	{
		addHeader(&header, lstContentEnv);
	}
	
	ASSERT(-1 == replaceStrFormat(&header, "", NL));
	errno = 0;
	int n = Write(Wfd, header, strlen(header));
	ASSERT(errno);

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
		total += n;
	}
	ASSERT(-1 == close(Wfd));

	//TRACE("\n%d %d >>\n", getpid(), time(NULL));

	int Rfd = open(Rfifo, O_RDONLY, 0);	
	ASSERT(-1 == Rfd);

	n_read	= 0;
	total	= 0;
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
			break;
		}

		errno = 0;
		int n = Write(STDOUT, buf, n_read);
		ASSERT(errno);
		total += n_read;
	}
	
	//TRACE("\n%d %d <<\n", getpid(), time(NULL));
	
	ASSERT(-1 == close(Rfd));
	ASSERT(-1 == lockf(Lfd, F_ULOCK, 0L));
	ASSERT(-1 == close(Lfd));
	ASSERT(-1 == unlink(Rfifo));

	lstFree(lstEnv);
	lstFree(lstHttpEnv);
	lstFree(lstContentEnv);
	gcClean(gc);
	
	return 0;
}

static void addHeader(char** header, char** lst)
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
		 
		ASSERT(-1 == replaceStrFormat(header, "", "%s: %s" NL, header1, header2));
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
