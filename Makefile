OBJS = fmstation.o
CC = gcc
APP = fmstation

CFLAGS = -O3
LDFLAGS = -lm

love: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(APP)

clean:
	rm -f $(OBJS) $(APP)
