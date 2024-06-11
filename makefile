CC = gcc
CFLAGS = -Wall -g -lrt -lpthread

all: rn

rn: PideShop.c HungryVeryMuch.c
	$(CC) PideShop.c -o a $(CFLAGS)
	$(CC) HungryVeryMuch.c -o b $(CFLAGS)

clean:
	find . -type f ! -name '*.c' ! -name 'makefile' ! -name '*.h' -delete

%:
	@:
