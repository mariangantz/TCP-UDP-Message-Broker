CFLAGS = -Wall -g -Werror -Wno-error=unused-variable -lm

all: server subscriber

common.o: common.c

messages.o: messages.c

server: server.c common.o messages.o -lm

subscriber: subscriber.c common.o messages.o -lm

.PHONY: clean run_server run_subscriber

clean:
	rm -rf server subscriber *.o *.dSYM
