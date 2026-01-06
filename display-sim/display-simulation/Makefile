# Tide Display Simulator - C Version
#
# Targets:
#   make       - Build with u8g2+SDL (pixel-perfect simulation)
#   make clean - Clean build artifacts
#   make run   - Build and run
#
# STM32 Build: Use PlatformIO (pio run)

CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include -I./u8g2/csrc
LDFLAGS = -lm

# SDL flags
SDL_CFLAGS = $(shell sdl2-config --cflags 2>/dev/null)
SDL_LDFLAGS = $(shell sdl2-config --libs 2>/dev/null)

SRC_DIR = src
BUILD_DIR = build
U8G2_DIR = u8g2/csrc

# Source files - split modules
SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/tide.c \
          $(SRC_DIR)/astro.c \
          $(SRC_DIR)/render.c \
          $(SRC_DIR)/timezone.c \
          $(SRC_DIR)/display_u8g2_sdl.c \
          $(SRC_DIR)/u8x8_d_sdl_128x296.c

# U8g2 library sources
U8G2_SOURCES = $(wildcard $(U8G2_DIR)/*.c)

ALL_SOURCES = $(SOURCES) $(U8G2_SOURCES)

TARGET = tide_sim

.PHONY: all clean run check-sdl

all: check-sdl $(BUILD_DIR) $(TARGET)

check-sdl:
	@if ! sdl2-config --version >/dev/null 2>&1; then \
		echo "ERROR: SDL2 not found. Install with: sudo apt install libsdl2-dev"; \
		exit 1; \
	fi

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(ALL_SOURCES:%.c=$(BUILD_DIR)/%.o)
	$(CC) $^ -o $@ $(LDFLAGS) $(SDL_LDFLAGS)

$(BUILD_DIR)/$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(U8G2_DIR)/%.o: $(U8G2_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) *.png
