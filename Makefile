CC=gcc
CXX=g++

ifeq ($(OS),Windows_NT)
TARGET=json_test.exe
else
TARGET=json_test
endif

all:
	$(CXX) -o $(TARGET) json_test_cpp.cpp