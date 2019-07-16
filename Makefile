.SUFFIXES : .c .o

CC = gcc
CFLAGS = -Wall
TARGET = ion
OBJECTS = main.o tun.o

$(TARGET) : $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) -lpthread

clean :
	rm $(OBJECTS) $(TARGET)
