CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -Werror -pedantic -pthread

all: proj2

proj2: proj2.o
	$(CC) proj2.o $(CFLAGS) -o proj2

proj2.o: proj2.c
	$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	rm *.o proj2

.PHONY: zip
zip:
	zip proj2.zip proj2.c Makefile
