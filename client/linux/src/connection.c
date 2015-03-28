/*
 *  connection.c
 *
 *  Created on: Sep 20, 2012
 *  Author: Andrey Zmushko
 */
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "trace.h"
#include "connection.h"

#define HTTP_MAXHDRS		64
#define MAX_HTTP_BUF		1024
#define TCP_KEEP_ALIVE		2

#define SOCKS4_V		4
#define SOCKS4_NOAUTH		0
#define SOCKS4_CONNECT		1
#define SOCKS4_REQUEST_LENGTH	9
#define SOCKS4_RESPOND_LENGTH	8
#define SOCKS4_SUCCESS		90

#define SOCKS5_V		5
#define SOCKS5_CONNECT		1
#define SOCKS5_BUFFER_LENGTH	513
#define SOCKS5_AUTH_LENGTH	2

#define SOCKS5_NOAUTH		0
#define SOCKS5_GSSAPI		1
#define SOCKS5_NAME_PASS	2

#define SOCKS5_HELLO_LENGTH	4
#define SOCKS5_ANSWER_LENGTH	2

#define SOCKS5_MAX_HOSTNAME	255
#define SOCKS5_IPV4		1
#define SOCKS5_DOMAIN		3
#define SOCKS5_IPV6		4

int Connection(const char*, const int, proxy_t*);
int CloseConnection(const int);

/**
 * @brief signal-safe basic IO operations
 * @param[in] *f pointer to read/write function
 * @param[in] fd socket
 * @param[in] b buffer
 * @param[in] n size
 * @return bytes processed if success or 0 if failure, see errno for detail
 */
static size_t AtomIO(ssize_t (*f)(int, void*, size_t), int fd, void* b, size_t n)
{
	char* s		= b;
	size_t pos	= 0;
	ssize_t res	= 0;

	while(n > pos)
	{
		res = (f)(fd, s + pos, n - pos);
		switch(res)
		{
			case -1:
				if(errno == EINTR || errno == EAGAIN)
				{
					continue;
				}
				return 0;
			case 0:
				errno = EPIPE;
				return pos;
			default:
				pos += (size_t)res;
		}
	}
	return pos;
}

/**
 * @brief fill sockaddr_in structure
 * @param[out] servaddr servaddr destination structure
 * @param[in] addr address
 * @param[in] port port
 * @return 0 if success or -1 if failure, see errno for detail
 */
static int PrepareSockaddr(struct sockaddr_in* servaddr, const char* addr, const int port)
{
	servaddr->sin_family	= AF_INET;
	servaddr->sin_port	= htons(port);

	struct hostent* h	= gethostbyname(addr);
	if(!h)
	{
		switch(h_errno)
		{
			case HOST_NOT_FOUND:
				ERROR("No such host is known.");
				break;
			case NO_DATA:
				ERROR("The server recognized the request and the name, but no address is available.");
				break;
			case TRY_AGAIN:
				ERROR("A temporary and possibly transient error occurred, such as a failure of a server to respond.");
				break;
			case NO_RECOVERY:
			default:
				ERROR("An unexpected server failure occurred which cannot be recovered.");
		}
		return -1;
	}

	servaddr->sin_addr = *((struct in_addr*)h->h_addr);

	return 0;
}

/**
 * @brief prepare sockaddr structure for SOCKS5 case
 * @param[out] addr servaddr destination structure
 * @param[in] host address
 * @param[in] port port
 * @param[in] addrlen sockaddr structure length
 * @return 0 if success or -1 if failure, see errno for detail
 */
static int GetAddrInfo(const char* host, const int port, struct sockaddr* addr, socklen_t addrlen)
{
	struct addrinfo	hints;
	bzero(&hints, sizeof(hints));
	struct addrinfo* res;

	char port_s[6];
	snprintf(port_s, 6, "%d", port);
	hints.ai_family		= PF_UNSPEC;
	hints.ai_socktype	= SOCK_STREAM;
	int r = getaddrinfo(host, port_s, &hints, &res);
	if(r)
	{
		TRACE("getaddrinfo() error:%s", gai_strerror(r));
		return -1;
	}
	if(addrlen < res->ai_addrlen)
	{
		ERROR("no memory to copy addr");
		freeaddrinfo(res);
		return -1;
	}

	memcpy(addr, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);

	return 0;
}

/**
 * @brief set necessary options for the socket
 * @param[in] sockfd socket descriptor
 * @return 0 if success or -1 if failure, see errno for detail
 */
static int Setsockopt(const int sockfd)
{
	int optval = 1;
	if(-1 == setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval))
	{
		ERROR("setsockopt()");
		return -1;
	}

	optval = TCP_KEEP_ALIVE;
	if(-1 == setsockopt(sockfd, SOL_TCP, TCP_KEEPIDLE, &optval, sizeof optval))
	{
		ERROR("setsockopt()");
		return -1;
	}

	return 0;
}

/**
 * @brief read one row from the socket
 * @param[in] s socket desription
 * @return NULL if failed, pointer to allocated buffer with row is success (need to be free)
 */
static char* GetSockRow(const int s)
{
	const int buf_size	= 256;
	char* row		= NULL;
	char buf[buf_size];
	memset(buf, 0, buf_size);
	ssize_t len		= 0;
	size_t total		= 0;

	while(0 < (len = recv(s, buf, buf_size - 1, MSG_PEEK)))
	{
		buf[len] = 0;
		char* p = strchr(buf, '\n');
		if(p)
		{
			memset(buf, 0, buf_size);
			len = recv(s, buf, p - buf + 1, 0);
			if(0 < len)
			{
				buf[len] = 0;
				char* t = realloc(row, total + len + 1);
				if(t)
				{
					row = t;
					memcpy(row + total, buf, len);
					*(row + total + len) = '\0';
					total += len;
				}
				else
				{
					free(row);
					row = NULL;
				}
			}
			else
			{
				free(row);
				row = NULL;
			}
			break;
		}
		else
		{
			len = recv(s, buf, buf_size - 1, 0);
			if(0 < len)
			{
				buf[len] = 0;
				char* t = realloc(row, total + len + 1);
				if(t)
				{
					row = t;
					memcpy(row + total, buf, len);
					*(row + total + len) = '\0';
					total += len;
				}
				else
				{
					free(row);
					row = NULL;
					break;
				}
			}
			else
			{
				free(row);
				row = NULL;
				break;
			}
		}
		memset(buf, 0, buf_size);
	}
	return row;
}

/**
 * @brief make connection through the proxy
 * @param[in] addr address of destination server
 * @param[in] port port of destination server
 * @param[in] proxy structure with proxy settings
 * @return socket or -1 if failure, see errno for detail
 */
static int SocksConnection(const char* addr, const int port, proxy_t* proxy)
{
	int proxyfd = Connection(proxy->addr,  proxy->port, NULL);
	if(proxyfd < 0)
	{
		return proxyfd;
	}

	if(proxy->type == HTTP)
	{
		unsigned char buf[MAX_HTTP_BUF] = {0};
		int r = 0;
		if(strcspn(addr, "\r\n\t []:") != strlen(addr))
		{
			ERROR("Invalid hostname");
			CloseConnection(proxyfd);
			return -1;
		}

		if(strchr(addr, ':') != NULL)
		{
			r = snprintf((char*)buf, sizeof(buf), "CONNECT [%s]:%d HTTP/1.0\r\n\r\n", addr, ntohs(port));
		}
		else
		{
			r = snprintf((char*)buf, sizeof(buf), "CONNECT %s:%d HTTP/1.0\r\n\r\n", addr, ntohs(port));
		}

		if(r == -1
			|| (size_t)r >= sizeof(buf))
		{
			ERROR("hostname too long");
			CloseConnection(proxyfd);
			return -1;
		}

		size_t wlen = strlen((char*)buf);

		if(wlen != AtomIO((ssize_t (*)(int, void*, size_t))write, proxyfd, buf, wlen))
		{
			ERROR("write()");
			CloseConnection(proxyfd);
			return -1;
		}

		for(r = 0;;r++)
		{
			char* row = GetSockRow(proxyfd);
			if(!row)
			{
				ERROR("read()");
				CloseConnection(proxyfd);
				return -1;
			}
			if(r == 0
				&& strncmp(row, "HTTP/1.0 200 ", 12) != 0)
			{
				ERROR("Proxy error: %s", row);
				CloseConnection(proxyfd);
				free(row);
				return -1;
			}
			if(row[0] == 0
				|| row[0] == 13)
			{
				free(row);
				break;
			}
			free(row);
		}

		return proxyfd;
	}

	if(proxy->type == SOCKS4)
	{
		struct sockaddr_in servaddr;
		bzero(&servaddr, sizeof(servaddr));
		if(-1 == PrepareSockaddr(&servaddr, addr, port))
		{
			CloseConnection(proxyfd);
			return -1;
		}
		unsigned char buf[SOCKS4_REQUEST_LENGTH] = {0};
		buf[0]	= SOCKS4_V;
		buf[1]	= SOCKS4_CONNECT;
		memcpy(buf + 2, &servaddr.sin_port, sizeof(servaddr.sin_port));
		memcpy(buf + 4, &servaddr.sin_addr, sizeof(servaddr.sin_addr));
		buf[8]	= SOCKS4_NOAUTH;

		if(SOCKS4_REQUEST_LENGTH != AtomIO((ssize_t (*)(int, void*, size_t))write, proxyfd, buf, SOCKS4_REQUEST_LENGTH))
		{
			ERROR("write()");
			CloseConnection(proxyfd);
			return -1;
		}

		if(SOCKS4_RESPOND_LENGTH != AtomIO(read, proxyfd, buf, SOCKS4_RESPOND_LENGTH))
		{
			ERROR("read()");
			CloseConnection(proxyfd);
			return -1;
		}

		if(buf[1] != SOCKS4_SUCCESS)
		{
			ERROR("SOCKS4 error %d", buf[1]);
			CloseConnection(proxyfd);
			return -1;
		}

		return proxyfd;
	}

	if(proxy->type == SOCKS5)
	{
		unsigned char buf[SOCKS5_BUFFER_LENGTH];
		bzero(&buf, SOCKS5_BUFFER_LENGTH);
		buf[0]	= SOCKS5_V;
		buf[1]	= SOCKS5_AUTH_LENGTH;
		buf[2]	= SOCKS5_NOAUTH;
		buf[3]	= SOCKS5_NAME_PASS;

		if(SOCKS5_HELLO_LENGTH != AtomIO((ssize_t (*)(int, void*, size_t))write, proxyfd, buf, SOCKS5_HELLO_LENGTH))
		{
			ERROR("write()");
			CloseConnection(proxyfd);
			return -1;
		}

		if(SOCKS5_ANSWER_LENGTH != AtomIO(read, proxyfd, buf, SOCKS5_ANSWER_LENGTH))
		{
			ERROR("read()");
			CloseConnection(proxyfd);
			return -1;
		}

		if(buf[0] != SOCKS5_V)
		{
			ERROR("There no socks5 on the other side");
			CloseConnection(proxyfd);
			return -1;
		}

		switch(buf[1])
		{
			case SOCKS5_NOAUTH:
				//ERROR("DEBUG:%s:%d: No authorization");
				break;
			case SOCKS5_GSSAPI:
				ERROR("SOCKS5 GSSAPI authentication is not supported");
				break;
			case SOCKS5_NAME_PASS:
			{
				//ERROR("DEBUG:%s:%d: Authorization through name and password");
				size_t userln		= 0;
				size_t passln		= 0;
				unsigned int len	= 0;
				if(proxy->user
					&& proxy->pass)
				{
					userln	= strlen(proxy->user);
					passln	= strlen(proxy->pass);
				}
				else
				{
					userln	= 0;
					passln	= 0;
				}

				/*   username/password request looks like
				* +----+------+----------+------+----------+
				* |VER | ULEN |  UNAME   | PLEN |  PASSWD  |
				* +----+------+----------+------+----------+
				* | 1  |  1   | 1 to 255 |  1   | 1 to 255 |
				* +----+------+----------+------+----------+
				*/
				buf[len++] = 1;    /* username/pw subnegotiation version */
				buf[len++] = (char)userln;
				memcpy(buf + len, proxy->user, (int)userln);
				len += userln;
				buf[len++] = (char)passln;
				memcpy(buf + len, proxy->pass, (int)passln);
				len += passln;

				if(len != AtomIO((ssize_t (*)(int, void*, size_t))write, proxyfd, buf, len))
				{
					ERROR("write()");
					CloseConnection(proxyfd);
					return -1;
				}

				if(SOCKS5_ANSWER_LENGTH != AtomIO(read, proxyfd, buf, SOCKS5_ANSWER_LENGTH))
				{
					ERROR("read()");
					CloseConnection(proxyfd);
					return -1;
				}

				if(buf[0] != SOCKS5_V)
				{
					ERROR("Answer broken on the other side");
					CloseConnection(proxyfd);
					return -1;
				}

				if(buf[1] != SOCKS5_NOAUTH)
				{
					ERROR("User was rejected by the SOCKS5 server answer[1]=%d", buf[1]);
					CloseConnection(proxyfd);
					return -1;
				}
				ERROR("DEBUG:%s:%d: User was granted by the SOCKS5 server answer[1]=%d", buf[1]);
			}
				break;
			default:
				ERROR("No present authorization-method");
				CloseConnection(proxyfd);
				return -1;
		}

		struct sockaddr_storage servaddr;
		bzero(&servaddr, sizeof(servaddr));
		struct sockaddr_in*	ipv4 = (struct sockaddr_in* )&servaddr;
		struct sockaddr_in6*	ipv6 = (struct sockaddr_in6*)&servaddr;
		if(-1 == GetAddrInfo(addr, port, (struct sockaddr*)&servaddr, sizeof(servaddr)))
		{
			CloseConnection(proxyfd);
			return -1;
		}
		size_t wlen	= 0;
		int hlen	= 0;
		switch(servaddr.ss_family) {
			case 0:
				/* domain */
				hlen = strlen(addr);
				if(hlen > SOCKS5_MAX_HOSTNAME)
				{
					ERROR("Host name too long for SOCKS5");
					CloseConnection(proxyfd);
					return -1;
				}
				buf[0] = SOCKS5_V;
				buf[1] = SOCKS5_CONNECT;
				buf[2] = 0;
				buf[3] = SOCKS5_DOMAIN;
				buf[4] = hlen;
				memcpy(buf + 5, addr, hlen);
				memcpy(buf + 5 + hlen, &ipv4->sin_port, sizeof(ipv4->sin_port));
				wlen = 7 + hlen;
				break;
			case AF_INET:
				/* IPv4 */
				buf[0] = SOCKS5_V;
				buf[1] = SOCKS5_CONNECT;
				buf[2] = 0;
				buf[3] = SOCKS5_IPV4;
				memcpy(buf + 4, &ipv4->sin_addr, sizeof(ipv4->sin_addr));
				memcpy(buf + 8, &ipv4->sin_port, sizeof(ipv4->sin_port));
				wlen = 10;
				break;
			case AF_INET6:
				/* IPv6 */
				buf[0] = SOCKS5_V;
				buf[1] = SOCKS5_CONNECT;
				buf[2] = 0;
				buf[3] = SOCKS5_IPV6;
				memcpy(buf + 4,  &ipv6->sin6_addr, sizeof(ipv6->sin6_addr));
				memcpy(buf + 20, &ipv6->sin6_port, sizeof(ipv6->sin6_port));
				wlen = 22;
				break;
			default:
				ERROR("Cannot determine address family");
				CloseConnection(proxyfd);
				return -1;
		}

		if(wlen != AtomIO((ssize_t (*)(int, void*, size_t))write, proxyfd, buf, wlen))
		{
			ERROR("write()");
			CloseConnection(proxyfd);
			return -1;
		}

		if(5 != AtomIO(read, proxyfd, buf, 5))
		{
			ERROR("read()");
			CloseConnection(proxyfd);
			return -1;
		}

		if(buf[0] != SOCKS5_V)
		{
			ERROR("There no socks5 on the other side");
			CloseConnection(proxyfd);
			return -1;
		}

		if(buf[1] != 0)
		{
			ERROR("Access denied answer=%d", buf[1]);
			switch(buf[1])
			{
				case 1:
					ERROR("general SOCKS server failure");
					break;
				case 2:
					ERROR("connection not allowed by ruleset");
					break;
				case 3:
					ERROR("Network unreachable");
					break;
				case 4:
					ERROR("Host unreachable");
					break;
				case 5:
					ERROR("Connection refused");
					break;
				case 6:
					ERROR("TTL expired");
					break;
				case 7:
					ERROR("Command not supported");
					break;
				case 8:
					ERROR("Address type not supported");
					break;
				default:
					ERROR("SOCKS Error unassigned");
					break;
			}
			CloseConnection(proxyfd);
			return -1;
		}

		//ERROR("DEBUG:%s:%d: Access granted in SOCKS5");
		unsigned int rlen = 0;
		switch(buf[3])
		{
			case 1:
				/* tail of IPv4(-1 byte) + port */
				rlen = 3 + 2;
				break;
			case 3:
				rlen = buf[4] + 2;
				break;
			case 4:
				/* tail of IPv6(-1 byte) + port */
				rlen = 15 + 2;
				break;
		}

		if(rlen != AtomIO(read, proxyfd, buf, rlen))
		{
			ERROR("read()");
			CloseConnection(proxyfd);
			return -1;
		}

		return proxyfd;
	}

	CloseConnection(proxyfd);
	return -1;
}

/**
 * @brief make datagram connection through proxy or direct depends by third parameter
 * @param[in] addr address to connect
 * @param[in] port port to connect
 * @param[in] proxy structure with proxy settings
 * @return socket connected or -1 if failure, see errno for detail
 */
int Connection(const char* addr, int port, proxy_t* proxy)
{
	if(!addr || port <= 0)
	{
		errno = EINVAL;
		return -1;
	}

	if(proxy && (proxy->type == HTTP || proxy->type == SOCKS4 || proxy->type == SOCKS5))
	{
		return SocksConnection(addr, port, proxy);
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	if(-1 == PrepareSockaddr(&servaddr, addr, port))
	{
		return -1;
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == sockfd)
	{
		ERROR("socket()");
		return -1;
	}

	if(-1 == Setsockopt(sockfd))
	{
		if(-1 == close(sockfd))
		{
			ERROR("close()");
		}
		return -1;
	}

	if(-1 == connect(sockfd, (const struct sockaddr*)&servaddr, sizeof(struct sockaddr_in)))
	{
		TRACE("connect()");
		if(-1 == close(sockfd))
		{
			ERROR("close()");
		}
		return -1;
	}
	return sockfd;
}

/**
 * @brief shutdown connection on an socket
 * @param[in] c socket to shutdown
 * @return 0 if success or -1 if failure, see errno for detail
 */
int CloseConnection(int c)
{
	if (c <= 0)
	{
		errno = EINVAL;
		return -1;
	}
	/*
	if (-1 == shutdown(c, SHUT_RDWR))
	{
		ERROR("shutdown()");
		return -1;
	}
	*/
	if (-1 == close(c))
	{
		ERROR("close()");
		return -1;
	}
	return 0;
}
