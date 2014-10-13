CC=gcc
CFLAGS=-I.
# Les .h
DEPS = common.h error.h
OBJ_S = sender.o common.o error.o
OBJ_R = receiver.o common.o error.o

all: sender receiver

send: sender
	./sender ::1 8888 --file in

receive: receiver
	./receiver ::1 8888 --file out

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

sender: $(OBJ_S)
	$(CC) -o $@ $^ $(CFLAGS)

receiver: $(OBJ_R)
	$(CC) -o $@ $^ $(CFLAGS)