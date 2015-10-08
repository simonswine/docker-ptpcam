CFLAGS=-c -Wall -I/usr/include/libusb-1.0/
CC=gcc
LDFLAGS=-lusb-1.0

all: cam2

cam2: cam2.o base64.o
	 $(CC) $(LDFLAGS) cam2.o base64.o -o cam2 -lusb-1.0

cam2.o: cam2.c
	$(CC) $(CFLAGS) cam2.c -o cam2.o 

base64.o: base64.c
	$(CC) -std=c99 $(CFLAGS) base64.c -o base64.o 

clean:
	rm *.o cam2
