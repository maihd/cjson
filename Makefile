CC=gcc
CFLAGS=-Wall -O0

.PHONY: clean

SRC=src/Json.c src/JsonUtils.c

ifeq ($(OS), Windows_NT)
CFLAGS+=-D_WIN32
endif

test:
	$(CC) -o json_test.exe src/Json_TokenTest.c $(SRC) $(CFLAGS)
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
	@rm -f Json.h JsonUtils.h Json.natvis

	@cat ./src/Json.h               							>> Json.h
	@echo ""                        							>> Json.h
	@echo "#ifdef JSON_IMPL"        							>> Json.h
	@echo ""                        							>> Json.h
	@cat ./src/Json.c               							>> Json.h
	@echo ""                        							>> Json.h
	@echo ""                        							>> Json.h
	@echo "#endif /* JSON_IMPL */"  							>> Json.h
	@echo ""                        							>> Json.h
	@echo "//! LEAVE AN EMPTY LINE HERE, REQUIRE BY GCC/G++"	>> Json.h
	@echo ""                        							>> Json.h

	@cat ./src/JsonUtils.h             							>> JsonUtils.h
	@echo ""                        							>> JsonUtils.h
	@echo "#ifdef JSON_UTILS_IMPL"        						>> JsonUtils.h
	@echo ""                        							>> JsonUtils.h	
	@cat ./src/JsonUtils.c               						>> JsonUtils.h
	@echo ""                        							>> JsonUtils.h
	@echo ""                        							>> JsonUtils.h
	@echo "#endif /* JSON_UTILS_IMPL */"						>> JsonUtils.h
	@echo ""                        							>> JsonUtils.h
	@echo "//! LEAVE AN EMPTY LINE HERE, REQUIRE BY GCC/G++"	>> JsonUtils.h
	@echo ""                        							>> JsonUtils.h

	@cat ./src/Json.natvis					>> Json.natvis

	@$(CC) -o json_build_test.exe json_build_test.c && rm json_build_test.exe 	\
		&& echo "Make single header library success." 							\
		|| echo "Make single header library failed."