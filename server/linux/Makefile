CC = gcc
CFLAGS = -g -fPIC
INC = -I../../common/linux/src
LIBLST = -L../../common/linux/src/liblst -I../../common/linux/src/liblst -llst

all : piercloud
	
piercloud : src/piercloud* ../../common/linux/src/trace.h liblst
	$(CC) ${CFLAGS} -o piercloud.cgi src/piercloud-cgi.c $(INC) $(LIBLST)

liblst : ../../common/linux/src/liblst/liblst.*
	cd ../../common/linux/src/liblst/ && make all

clean :
	-rm *.o piercloud.cgi
