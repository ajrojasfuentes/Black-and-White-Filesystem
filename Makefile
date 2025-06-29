# Makefile for BWFS minimal build

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude
LDFLAGS =

SRC_DIR = src
CLI_DIR = $(SRC_DIR)/cli
CORE_DIR = $(SRC_DIR)/core
UTIL_DIR = $(SRC_DIR)/util

OBJ_DIR = build/obj
BIN_DIR = build/bin

UTIL_SRCS = $(UTIL_DIR)/bmp_io.c
CORE_SRCS = $(CORE_DIR)/bwfs_common.c
CLI_SRCS  = $(CLI_DIR)/mkfs_bwfs.c

UTIL_OBJS = $(patsubst $(UTIL_DIR)/%.c,$(OBJ_DIR)/%.o,$(UTIL_SRCS))
CORE_OBJS = $(patsubst $(CORE_DIR)/%.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))
CLI_OBJS  = $(patsubst $(CLI_DIR)/%.c,$(OBJ_DIR)/%.o,$(CLI_SRCS))

OBJS = $(UTIL_OBJS) $(CORE_OBJS) $(CLI_OBJS)

TARGETS = $(BIN_DIR)/mkfs_bwfs

all: dirs $(TARGETS)

# Build mkfs_bwfs
$(BIN_DIR)/mkfs_bwfs: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

# Compile object files for util, core, cli
$(OBJ_DIR)/%.o: $(UTIL_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(CORE_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(CLI_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Create necessary directories
dirs:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean dirs
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
