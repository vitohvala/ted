#LIBS
FLAGS= -Wall -Wextra -pedantic -std=c99
SRC=kaczynski.c
CC=cc
EXE=ted
all: $(SRC)
	$(CC) $(SRC) -o $(EXE) $(FLAGS)
