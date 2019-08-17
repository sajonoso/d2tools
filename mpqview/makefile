# Simple make file for mingw32

CC=gcc
CFLAGS=-I. -s
DEPS = Dclib.h Wav_unp.h
OBJ = Dcl_tbl.o Wav_unp.o Explode.o MpqView.o
OUTPUT = mpqview.exe

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(OUTPUT): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
	strip --strip-all $(OUTPUT)
