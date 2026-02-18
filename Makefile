# AMACS Makefile

CC = m68k-amigaos-gcc
CFLAGS = -O2 -noixemul -Wall -Wno-pointer-sign \
         -I/opt/amiga/m68k-amigaos/ndk-include
LFLAGS = -lamiga

SRC_DIR = src
BUILD_DIR = build

# All .c files in src/
SRC = $(wildcard $(SRC_DIR)/*.c)

# Corresponding .o files in build/
OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

TARGET = $(BUILD_DIR)/amacs

all: $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean