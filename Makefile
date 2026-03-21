CXX = g++
CXXFLAGS = -std=c++17 -pthread -I include -Wall -Wextra

SRC = $(wildcard src/*.cpp) main.cpp
OBJ = $(SRC:.cpp=.o)

TARGET = main

all: .env $(TARGET)

.env:
	cp .env.example .env
	@echo "Created .env from .env.example"

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(TARGET)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: 
	./$(TARGET) $(ARGS)
