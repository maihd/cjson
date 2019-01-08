CC=gcc

.PHONY: clean

test:
	$(CC) -o json_test.exe json_test.c -Wall -O0

unit_test:
	$(CC) -o json_unit_test.exe json_unit_test.c -Wall -O0 -DJSON_IMPL
	./json_unit_test.exe $(wildcard ./testdb/json/*.json)

unit_test_dbg:
	$(CC) -c json.c -Wall -O -g
	$(CC) -c json_unit_test.c -Wall -O -g
	$(CC) -o json_unit_test.exe json.o json_unit_test.o -Wall -O -g
	gdb json_unit_test

clean:
	rm -rf *.o *.exe