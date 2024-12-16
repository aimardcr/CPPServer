# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -I./source/include
LDFLAGS := -pthread

# Project structure
SRC_DIR := source/src
INCLUDE_DIR := source/include
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

# Source files and corresponding object files
SOURCES := $(SRC_DIR)/MimeType.cpp \
           $(SRC_DIR)/Utils.cpp \
           $(SRC_DIR)/HttpRequest.cpp \
           $(SRC_DIR)/HttpResponse.cpp \
           $(SRC_DIR)/HttpServer.cpp
OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Target executable and library
LIBRARY := $(BIN_DIR)/libserver.a
EXECUTABLE := $(BIN_DIR)/main
TARGET_BINARY := ./server

# Default target
default: $(TARGET_BINARY)

# Create directories if needed
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build library
$(LIBRARY): $(OBJ_DIR) $(OBJECTS) $(BIN_DIR)
	ar rcs $@ $(OBJECTS)

# Build executable
$(EXECUTABLE): main.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Copy binary to target location
$(TARGET_BINARY): $(EXECUTABLE)
	cp $(EXECUTABLE) $(TARGET_BINARY)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Run the application
run: $(TARGET_BINARY)
	cd $(TARGET_BINARY) && chmod +x main && ./main

# Clean up
clean:
	rm -rf $(BUILD_DIR) $(TARGET_BINARY)

.PHONY: default clean run