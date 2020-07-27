CC	 	= g++
LD	 	= g++
CFLAGS	 	= -Wall -g

all:	server client lib

server: server.cpp
	$(CC) $(CFLAGS) $(LIB) server.cpp  -I/usr/include/mysql \
  -o server -L/usr/lib64/ -lstdc++ \
  -L/usr/lib/mysql/ -lmysqlclient -IC:/lua-5.3.4/install/include/ -LC:/lua-5.3.4/install/lib/ -llua -ldl -Wl,-E

client: client.cpp
	$(CC) $(CFLAGS) $(LIB) -o client client.cpp

lib: mylib.cpp
	$(CC) $(CFLAGS) mylib.cpp -shared -fpic -IC:/lua-5.3.4/install/include/ -o mylib.so

clean:
	rm -f *.o
	rm -f *~
	rm -f core.*.*
	rm -f server
	rm -f client
	rm -f *.so

