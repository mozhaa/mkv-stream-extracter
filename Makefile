CC = g++
CFLAGS = -std=c++2a -g -lboost_regex
EXE = subex
OBJ = main.o
SRCDIR = src
EXT = cpp

%.o: $(SRCDIR)/%.$(EXT)
	$(CC) -c -o $@ $< $(CFLAGS)

all: $(OBJ)
	$(CC) -o $(EXE) $^ $(CFLAGS)

clean:
	rm -f $(OBJ) $(EXE)