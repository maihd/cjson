CC=gcc

.PHONY: clean

test:
	$(CC) -o json_test.exe src/json_test.c -Wall -O0

unit_test:
	$(CC) -c src/json.c -Wall -O0
	$(CC) -c src/json_unit_test.c -Wall -O0
	$(CC) -o json_unit_test.exe json.o json_unit_test.o -Wall -O0
	./json_unit_test.exe $(wildcard ./testdb/json/*.json)

unit_test_dbg:
	$(CC) -c src/json.c -Wall -O0 -g
	$(CC) -c src/json_unit_test.c -Wall -O0 -g
	$(CC) -o json_unit_test.exe json.o json_unit_test.o -Wall -O0 -g
	gdb json_unit_test

clean:
	rm -rf *.o *.exe