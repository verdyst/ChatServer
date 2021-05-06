FLAGS = -Wall -g
CC = gcc $(CFLAGS)

PROGRAMS = \
	   bl_server \
	   bl_client \
	   bl_showlog \

all: $(PROGRAMS)

bl_server : bl_server.o server_funcs.o util.o
	$(CC) -g -o bl_server $^ -lpthread
	@echo bl_server is ready

bl_client : bl_client.o simpio.o
	$(CC) -o bl_client $^ -lpthread
	@echo bl_client is ready

bl_showlog : bl_showlog.o
	$(CC) -o bl_showlog $^
	@echo bl_showlog is ready

bl_server.o : bl_server.c blather.h
	$(CC) -g -c bl_server.c

server_funcs.o : server_funcs.c blather.h
	$(CC) -c server_funcs.c

util.o : util.c blather.h
	$(CC) -c util.c

bl_client.o : bl_client.c blather.h
	$(CC) -c bl_client.c

simpio.o : simpio.c blather.h
	$(CC) -c simpio.c

bl_showlog.o : bl_showlog.c blather.h
	$(CC) -c bl_showlog.c

clean:
	@echo Cleaning up object files
	rm -f *.o bl_server
	rm -f *.o bl_client

clean_logs:
	@echo Begone log files!
	rm -f *.log

