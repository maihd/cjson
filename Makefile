CC=gcc

.PHONY: clean

test:
	$(CC) -o json_test.exe src/json_test.cc -Wall -O0

unit_test:
	$(CC) -o json_unit_test.exe src/json_unit_test.cc -Wall -O0
	./json_unit_test.exe $(wildcard ./testdb/json/*.json)

unit_test_dbg:
	$(CC) -o json_unit_test.exe src/json_unit_test.cc -Wall -O0 -g
	gdb json_unit_test

clean:
	rm -rf *.o *.exe

lib:
	rm -f json.h

	cat ./src/json.h                >> json.h
	echo ""                         >> json.h
	echo "#ifdef JSON_IMPL"         >> json.h
	echo ""                         >> json.h	
	cat ./src/json.c                >> json.h
	echo ""                         >> json.h
	echo ""                         >> json.h
	echo "#endif /* JSON_IMPL */"   >> json.h
	echo ""                         >> json.h