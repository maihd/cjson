CC=gcc

ifeq ($(OS),Windows_NT)
TARGET=json_test.exe
else
TARGET=json_test
endif

all:
	$(CC) -o $(TARGET) json_test.c