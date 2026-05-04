BUILD_DIR ?= build
MYSQL_ROOT ?=

ifeq ($(OS),Windows_NT)
GENERATOR ?= -G "Visual Studio 17 2022" -A x64
MYSQL_FLAG := $(if $(strip $(MYSQL_ROOT)),-DMYSQL_ROOT="$(MYSQL_ROOT)",)

.PHONY: all clean

all:
	cmake -S . -B $(BUILD_DIR) $(GENERATOR) $(MYSQL_FLAG)
	cmake --build $(BUILD_DIR) --config Debug
	.\$(BUILD_DIR)\Debug\campus_trade_web.exe

else
CXX ?= g++
BIN_DIR := $(BUILD_DIR)
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
INCLUDES := -Iinclude $(if $(strip $(MYSQL_ROOT)),-I$(MYSQL_ROOT)/include,)
MYSQL_LIB_NAME ?= mysqlclient
LDFLAGS := $(if $(strip $(MYSQL_ROOT)),-L$(MYSQL_ROOT)/lib,) -l$(MYSQL_LIB_NAME)
TARGET := $(BIN_DIR)/campus_trade_web

.PHONY: all clean

all: $(TARGET)
	

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(TARGET): src/HttpServer.cpp src/HttpServerPages.cpp src/HttpServerStatic.cpp src/http_main.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

endif
