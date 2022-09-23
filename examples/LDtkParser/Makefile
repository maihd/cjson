CC 		= gcc

CFLAGS	= -Wall -Wextra -I./3rdparty/raylib-mingw64/include -I../../src
LFLAGS	= ./3rdparty/raylib-mingw64/lib/libraylib.a

SRC		= LDtkParser.c Main.c ../../src/Json.c
EXE		= bin/LDtkParserTest.exe

LFLAGS 	+= -lgdi32 -luser32 -lwinmm

all:
	$(CC) -o $(EXE) $(SRC) $(CFLAGS) $(LFLAGS)
	./$(EXE)

clean:
	rm ./$(EXE)