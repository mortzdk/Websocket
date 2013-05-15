CC 		= gcc
CFLAGS 	= -Wall -Wextra -Werror -pedantic -ggdb -DRUPIFY -g

INCL 	= Handshake.c
OBJECTS = Errors.o Datastructures.o Communicate.o sha1.o md5.o base64.o utf8.o
EXEC 	= Websocket

.PHONY: Websocket

all: clean Websocket

Websocket: $(OBJECTS) 
	$(CC) $(CFLAGS) $(INCL) $(OBJECTS) -lpthread $(EXEC).c -o $(EXEC) -std=c99

clean:
	rm -f $(EXEC) *.o

run: all
	./$(EXEC) $(PORT)

valgrind: all
	valgrind --leak-check=full --log-file="LOG" --track-origins=yes --show-reachable=yes ./$(EXEC) $(PORT)

base64.o: base64.c base64.h
	$(CC) $(CFLAGS) -c base64.c

md5.o: md5.c md5.h
	$(CC) $(CFLAGS) -c md5.c

utf8.o: utf8.c utf8.h
	$(CC) $(CFLAGS) -c utf8.c

sha1.o: sha1.c sha1.h
	$(CC) $(CFLAGS) -c sha1.c

Communicate.o: Communicate.c Communicate.h Datastructures.h
	$(CC) $(CFLAGS) -c Communicate.c

Datastructures.o: Datastructures.c Datastructures.h
	$(CC) $(CFLAGS) -c Datastructures.c

Errors.o: Errors.c Errors.h Datastructures.h
	$(CC) $(CFLAGS) -c Errors.c
