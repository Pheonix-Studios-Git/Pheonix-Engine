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

GLOBAL_MACROS := 
POSIX := 0

ifeq ($(OS),linux)
    POSIX := 1
endif
ifeq ($(OS),darwin)
    POSIX := 1
endif
ifeq ($(OS),freebsd)
    POSIX := 1
endif

ifeq ($(POSIX), 1)
	GLOBAL_MACROS += -D_POSIX_C_SOURCE=200809L
endif

OUT_DIR := $(BIN_DIR)/$(OS)/$(ARCH)
TARGET := $(OUT_DIR)/$(PROJECT)

# === Flags ===
CSTD := -std=c11
WARN := -Wall -Wextra -Wpedantic -Wstrict-prototypes
DEFS :=
INCS := -I$(INC_DIR)

OPT_LEVEL ?= 0
TARGET_CPU ?= generic
EXTRA ?=

CPU_FLAGS :=

ifneq ($(TARGET_CPU),generic)
    CPU_FLAGS += -march=$(TARGET_CPU) -mtune=$(TARGET_CPU)
endif

CFLAGS := $(CSTD) $(WARN) $(DEFS) $(INCS) -fno-strict-aliasing $(GLOBAL_MACROS) -O$(OPT_LEVEL) $(CPU_FLAGS) $(EXTRA)
LDFLAGS := $(EXTRA) -lX11 -lGL -lGLU -lGLEW -lm -lpng -lXrender -lassimp -ldl

# === Sources ===
SRC := $(shell find $(SRC_DIR) -type f -name '*.c')
OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC))

# === Rules ===
.PHONY: all clean help dirs

all: dirs $(TARGET)

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

