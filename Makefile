
CFLAGS = -g

OBJ = debug.o emulator.o support.o

debug96: $(OBJ)
	$(CC) $(CFLAGS) -o debug96 $(OBJ)

