CXX=g++
CXXFLAGS=-g -lstdc++

SRC=$(wildcard *.cpp)
OBJ=$(SRC:%.cpp=%.o)

all: $(OBJ)
	$(CXX) -o $(BIN) $^

%.o: %.c
	$(CXX) $@ -c $<

clean:
	rm -f server
	rm -f client
	rm -f *.o

build: server client

.SUFFIXES:
.SUFFIXES: .cpp
