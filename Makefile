# ASCII Art OFX Plugin - Linux Makefile
CXX ?= g++
CXXFLAGS ?= -O3 -fPIC -std=c++17 -Wall -Wno-unused-function -fopenmp
INCLUDES = -Iinclude -Isrc
LDFLAGS = -shared -ldl -fopenmp

BUNDLE_DIR = AsciiArt.ofx.bundle
TARGET = $(BUNDLE_DIR)/Contents/Linux-x86-64/AsciiArt.ofx
INSTALL_DIR ?= /usr/OFX/Plugins

all: $(TARGET)

$(TARGET): src/ascii_art_ofx.cpp src/glyph_atlas.h
	@mkdir -p $(BUNDLE_DIR)/Contents/Linux-x86-64
	@mkdir -p $(BUNDLE_DIR)/Contents/Resources
	@cp -n Goliath.ttf $(BUNDLE_DIR)/Contents/Resources/ 2>/dev/null || true
	$(CXX) $(CXXFLAGS) $(INCLUDES) src/ascii_art_ofx.cpp $(LDFLAGS) -o $(TARGET)
	@echo ""
	@echo "Build successful: $(TARGET)"

install: all
	@echo "Installing to $(INSTALL_DIR)..."
	@mkdir -p $(INSTALL_DIR)
	@cp -r $(BUNDLE_DIR) $(INSTALL_DIR)/
	@echo "Installation complete! Restart DaVinci Resolve."

install-user: all
	@echo "Installing to ~/.ofx/Plugins (user-only)..."
	@mkdir -p ~/.ofx/Plugins
	@cp -r $(BUNDLE_DIR) ~/.ofx/Plugins/
	@echo "Installation complete! Restart DaVinci Resolve."

clean:
	rm -f $(TARGET)

.PHONY: all install install-user clean
