CC=gcc
CFLAGS=-Wall -O0

.PHONY: clean

SRC=src/Json.c src/JsonEx.c

ifeq ($(OS), Windows_NT)
CFLAGS+=-D_WIN32
endif

test:
	$(CC) -o json_test.exe src/Json_TokenTest.c src/Json.c src/JsonEx.c $(CFLAGS)
	./json_test.exe

unit_test:
	$(CC) -o json_unit_test.exe src/json_unit_test.c $(SRC) $(CFLAGS)
	./json_unit_test.exe $(wildcard ./testdb/json/*.json)

unit_test_dbg:
	$(CC) -o json_unit_test.exe src/json_unit_test.c $(SRC) $(CFLAGS) -g
	gdb json_unit_test

clean:
	rm -rf *.o *.exe

lib:
	@rm -f Json.h JsonEx.h

	@echo "#ifndef __JSON_H__"		>> Json.h
	@echo "#define __JSON_H__"		>> Json.h
	@cat ./src/Json.h               >> Json.h
	@echo "#endif /* __JSON_H__ */" >> Json.h
	@echo ""                        >> Json.h
	@echo "#ifdef JSON_IMPL"        >> Json.h
	@echo ""                        >> Json.h	
	@cat ./src/Json.c               >> Json.h
	@echo ""                        >> Json.h
	@echo ""                        >> Json.h
	@echo "#endif /* JSON_IMPL */"  >> Json.h
	@echo ""                        >> Json.h

	@echo "#ifndef __JSON_EX_H__"		>> JsonEx.h
	@echo "#define __JSON_EX_H__"		>> JsonEx.h
	@cat ./src/JsonEx.h             	>> JsonEx.h
	@echo "#endif /* __JSON_EX_H__ */" 	>> JsonEx.h
	@echo ""                        	>> JsonEx.h
	@echo "#ifdef JSON_EX_IMPL"        	>> JsonEx.h
	@echo ""                        	>> JsonEx.h	
	@cat ./src/JsonEx.c               	>> JsonEx.h
	@echo ""                        	>> JsonEx.h
	@echo ""                        	>> JsonEx.h
	@echo "#endif /* JSON_EX_IMPL */"  	>> JsonEx.h
	@echo ""                        	>> JsonEx.h

	@gcc -o json_build_test.exe json_build_test.c && rm json_build_test.exe \
		&& echo "Make single header library success." \
		|| echo "Make single header library failed."