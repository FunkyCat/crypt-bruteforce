all: brute

brute: brute.o shared.o single.o multi.o client.o server.o server_async.o client_async.o server_epoll.o

CFLAGS += -O2 -Wall -g `xml2-config --cflags`
LDLIBS += -lcrypt -lpthread `xml2-config --libs`
