CC=gcc
PROJ=proj2
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic
LDFLAGS=-lrt -pthread

all: $(proj) run

$(PROJ): $(PROJ).c
	$(CC) $(CFLAGS) $(PROJ).c -o $(PROJ) $(LDFLAGS)
	

run: $(PROJ)
	./$(PROJ) 1 2 0 0 0 0

clean:
	rm $(PROJ)
