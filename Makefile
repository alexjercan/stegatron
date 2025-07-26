CC = clang
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lm
BUILD_DIR = build
SRC_DIR = src

$(BUILD_DIR)/steg: $(BUILD_DIR)/main.o $(BUILD_DIR)/steg.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c $(SRC_DIR)/aids.h $(SRC_DIR)/argparse.h $(SRC_DIR)/steg.h $(SRC_DIR)/stb_image.h $(SRC_DIR)/stb_image_write.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/steg.o: $(SRC_DIR)/steg.c $(SRC_DIR)/steg.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean up the build directory
clean:
	rm -rf $(BUILD_DIR)

.PHONY: clean

