# Simple make file for mingw32

CC=gcc
CFLAGS=-I. -s
DEPS =
OBJ = dc6con.o
OUTPUT = dc6con.exe

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OUTPUT): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	strip --strip-all $(OUTPUT)
