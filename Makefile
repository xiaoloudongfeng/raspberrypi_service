src := $(shell ls *.c)
objs := $(patsubst %.c, %.o, $(src))
CC = gcc
CPPFLAGS = -Wall
LDFLAGS = -lbcm2835 -lpthread -lm -lssl -lcrypto

lcd: $(objs)
	$(CC) $^ -o $@ $(LDFLAGS) 

%.o: %.c
	$(CC) $(CPPFLAGS) -c -o $@ $<

clean:
	rm *.o lcd
