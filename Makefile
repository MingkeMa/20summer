CC	 	= g++
LD	 	= g++
CFLAGS	 	= -Wall -g

LDFLAGS	 	=
DEFS 	 	=

all:	server client 

server: server.cpp
	# $(CC) $(DEFS) $(CFLAGS) $(LIB) -o server server.cpp `mysql_config --cflags --libs`
	$(CC) $(DEFS) $(CFLAGS) $(LIB) server.cpp  -I/usr/include/mysql \
  -o server -L/usr/lib64/ -lstdc++ \
  -L/usr/lib/mysql/ -lmysqlclient

client: client.cpp
	$(CC) $(DEFS) $(CFLAGS) $(LIB) -o client client.cpp

#name_addr:	name_addr.c
#	$(CC) $(DEFS) $(CFLAGS) $(LIB) -o name_addr name_addr.c

clean:
	rm -f *.o
	rm -f *~
	rm -f core.*.*
	rm -f server
	rm -f client

