CXX = g++
CXXFLAGS = -std=c++17 -pthread -I include -Wall -Wextra -O2 -march=native -MMD -MP


TARGET = main
OBJ_DIR = obj
SRC_DIR = src

SRC = $(wildcard $(SRC_DIR)/*.cpp) main.cpp
OBJ = $(addprefix $(OBJ_DIR)/,$(notdir $(SRC:.cpp=.o)))
DEPS = $(OBJ:.o=.d)


all: .env $(TARGET)

.env:
	cp .env.example .env
	@echo "Created .env from .env.example"

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(TARGET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/main.o: main.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

test: $(TARGET)
	bash tests/run_tests.sh

run:
	./$(TARGET) $${ARGS}

-include $(DEPS)

.PHONY: all clean test run
