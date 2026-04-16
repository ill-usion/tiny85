CC=cc
SRCS=main.c
CCFLAGS=-O2 -Wall

tiny85: main.c tiny85.h
	$(CC) $(CCFLAGS) $(SRCS) -o tiny85

run: tiny85
	./tiny85

.PHONY: clean
clean:
	rm tiny85
