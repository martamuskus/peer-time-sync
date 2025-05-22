CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2
CXXFLAGS = $(CFLAGS) -std=c++17

.PHONY: all clean

TARGET1 = peer-time-sync

all: $(TARGET1)

err.o: err.cpp err.h
InputParser.o: InputParser.cpp InputParser.h
NetworkNode.o: NetworkNode.cpp NetworkNode.h config.h err.h
peer-time-sync.o: peer-time-sync.cpp err.h utils.h InputParser.h NetworkNode.h config.h
utils.o: utils.cpp err.h

$(TARGET1): $(TARGET1).o err.o utils.o InputParser.o NetworkNode.o
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET1) $(TARGET2) *.o *~
