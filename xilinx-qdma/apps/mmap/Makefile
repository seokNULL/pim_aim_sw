CC = gcc
CFLAGS = -W
TARGET = show_mem
OBJECTS = show_mem.o

all : $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf show_mem
	rm -rf *.o
