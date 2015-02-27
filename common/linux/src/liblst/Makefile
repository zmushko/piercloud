#CC = gcc
CFLAGS = -g -fPIC

all-arm : 
# Build armel
	CC=arm-linux-gnueabi-gcc-4.7 \
	CXX=arm-linux-gnueabi-g++-4.7 \
	STRIP=arm-linux-gnueabi-strip \
	LIBTOOL=/usr/arm-linux-gnueabi/bin/libtool \
	LDFLAGS="-L/usr/arm-linux-gnueabi/lib -L/home/andrei/Work/RAIDiator/RAIDiator-Platform/trunk/readynasd-client"	 \
	PKG_CONFIG_PATH=/usr/arm-linux-gnueabi/lib/pkgconfig \
	ARCH=arm \
	make all

all : liblst-a test

static : liblst-a

dynamic : liblst-so

test : test.c 
	$(CC) ${CFLAGS} -o test test.c -L. -llst 

liblst-so : liblst.c 
	$(CC) ${CFLAGS} -shared -o liblst.so liblst.c $(LDFLAGS)

liblst-a : liblst.c 
	$(CC) ${CFLAGS} -c liblst.c $(LDFLAGS)
	ar cr liblst.a liblst.o

clean :
	-rm *.bak *.o *.txt *.out core *.so test *.a
