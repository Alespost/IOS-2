CC=gcc
PROJ=proj2
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic

all: $(proj) run

$(PROJ): $(PROJ).c
	$(CC) $(CFLAGS) $(PROJ).c -o $(PROJ)

run: $(PROJ)
	./$(PROJ) 3 2 3 4 5 6

clean:
	rm $(PROJ)