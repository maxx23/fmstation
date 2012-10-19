OBJS = fmstation.o
CC = gcc
APP = fmstation

CFLAGS = -O3 -pipe -mtune=amdfam10 -fomit-frame-pointer
LDFLAGS = -g -lm

love: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(APP)

clean:
	rm -f $(OBJS) $(APP)
