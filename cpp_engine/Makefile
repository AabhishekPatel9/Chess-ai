CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra -DNDEBUG -flto
TARGET = chess_engine.exe

SRCS = board.cpp search.cpp main.cpp
OBJS = $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	del /Q *.o $(TARGET) 2>nul

.PHONY: clean
