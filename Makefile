CC=gcc
CFLAGS=-Wall -O0

.PHONY: clean

test:
	$(CC) -o json_test.exe src/Json_TokenTest.c $(CFLAGS)
	./json_test.exe

unit_test:
	$(CC) -o json_unit_test.exe src/json_unit_test.c src/Json.c $(CFLAGS)
	./json_unit_test.exe $(wildcard ./testdb/json/*.json)

unit_test_dbg:
	$(CC) -o json_unit_test.exe src/json_unit_test.c $(CFLAGS) -g
	gdb json_unit_test

clean:
	rm -rf *.o *.exe

lib:
	@rm -f Json.h

	@cat ./src/Json.h                >> Json.h
	@echo ""                         >> Json.h
	@echo "#ifdef JSON_IMPL"         >> Json.h
	@echo ""                         >> Json.h	
	@cat ./src/Json.c                >> Json.h
	@echo ""                         >> Json.h
	@echo ""                         >> Json.h
	@echo "#endif /* JSON_IMPL */"   >> Json.h
	@echo ""                         >> Json.h

	@gcc -o json_build_test.exe json_build_test.c && rm json_build_test.exe \
		&& echo "Make single header library success." \
		|| echo "Make single header library failed."