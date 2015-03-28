#ifndef __CONNECTION_H
#define __CONNECTION_H

typedef enum {
	HTTPS,
	HTTP,
	SOCKS4,
	SOCKS5
} proxy_type_t;

typedef struct {
	proxy_type_t	type;
	char*		addr;
	unsigned int	port;
	char*		user;
	char*		pass;
} proxy_t;

int Connection(const char* addr, int port, proxy_t* proxy);
int CloseConnection(int c);


#endif
