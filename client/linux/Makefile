CC = gcc
CFLAGS = -g -fPIC
INC = -I../../common/linux/src
LIBLST = -L../../common/linux/src/liblst -I../../common/linux/src/liblst -llst -lssl -lcrypto

all : piercloud
	
piercloud : src/piercloud.* ../../common/linux/src/trace.h liblst socket connection authorization
	$(CC) ${CFLAGS} -o piercloud src/piercloud.c connection.o authorization.o socket.o src/base64/base64.o $(INC) $(LIBLST)

liblst : ../../common/linux/src/liblst/liblst.*
	cd ../../common/linux/src/liblst/ && make all

socket : src/socket.* 
	$(CC) ${CFLAGS} -c src/socket.c $(INC) $(LIBLST)

connection : src/connection.* 
	$(CC) ${CFLAGS} -c src/connection.c $(INC) $(LIBLST)

authorization : src/authorization.* base64
	$(CC) ${CFLAGS} -c src/authorization.c $(INC) $(LIBLST)

base64 : src/base64/base64.*
	cd src/base64/ && make all

clean :
	-rm *.o piercloud
