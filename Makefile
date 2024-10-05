all: proxy.o connection.o queue.o
	gcc -o app proxy.o connection.o queue.o

%: %.c
	gcc -c %.c 