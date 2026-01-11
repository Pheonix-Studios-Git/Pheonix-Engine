# =========================
# Pheonix Engine Makefile
# =========================

# -------- Project --------
PROJECT := pheonix-engine

SRC_DIR := src
INC_DIR := inc
BUILD_DIR := build
BIN_DIR := bin

# === Toolchain ===
CC := gcc
AR := ar

# === Host Detection ===
OS := $(shell uname -s | tr '[:upper:]' '[:lower:]')
ARCH := $(shell uname -m)

# Normalize arch names a bit
ifeq ($(ARCH),x86_64)
    ARCH := x86_64
endif
ifeq ($(ARCH),i686)
    ARCH := x86
endif

OUT_DIR := $(BIN_DIR)/$(OS)/$(ARCH)
TARGET := $(OUT_DIR)/$(PROJECT)

# === Build Mode ===
MODE ?= debug

# === Flags ===
CSTD := -std=c11
WARN := -Wall -Wextra -Wpedantic -Wstrict-prototypes
DEFS :=
INCS := -I$(INC_DIR)

COMMON_CFLAGS := $(CSTD) $(WARN) $(DEFS) $(INCS) -fno-strict-aliasing
LDFLAGS := -lX11

ifeq ($(MODE),debug)
    CFLAGS := $(COMMON_CFLAGS) -g -O0 \
        -fsanitize=address,undefined \
        -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address,undefined
else ifeq ($(MODE),release)
    # IMPORTANT Note: intentionally unoptimized
    CFLAGS := $(COMMON_CFLAGS) -O0 -DNDEBUG
else
    $(error Unknown MODE '$(MODE)'. Use MODE=debug or MODE=release)
endif

# === Sources ===
SRC := $(shell find $(SRC_DIR) -type f -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

# === Rules ===
.PHONY: all debug release clean help dirs

all: debug

debug:
	@$(MAKE) MODE=debug build

release:
	@$(MAKE) MODE=release build

build: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OUT_DIR)

$(TARGET): $(OBJ)
	@echo "Linking $@"
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning build artifacts"
	@rm -rf $(BUILD_DIR) $(BIN_DIR)

help:
	@echo "Pheonix Engine Build System"
	@echo ""
	@echo "Usage:"
	@echo "  make                Build debug (default)"
	@echo "  make debug          Build debug mode"
	@echo "  make release        Build release mode (UNOPTIMIZED)"
	@echo "  make clean          Remove all build artifacts"
	@echo ""
	@echo "Variables:"
	@echo "  MODE=debug|release  Select build mode"
	@echo "  CC=<compiler>       Override C compiler"
	@echo ""
	@echo "Output:"
	@echo "  $(BIN_DIR)/<os>/<arch>/$(PROJECT)"

