CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2
CXXFLAGS = $(CFLAGS) -std=c++17

.PHONY: all clean

TARGET1 = peer-time-sync

all: $(TARGET1)

peer-time-sync.o: peer-time-sync.cpp err.h utils.h
err.o: err.cpp err.h
utils.o: utils.cpp utils.h
read_write.o: read_write.c read_write.h
InputParser.o: InputParser.cpp InputParser.h
NetworkNode.o: NetworkNode.cpp NetworkNode.h read_write.h

$(TARGET1): $(TARGET1).o err.o utils.o InputParser.o NetworkNode.o read_write.o
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET1) $(TARGET2) *.o *~
