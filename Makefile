# Libraries.
STATIC_LIBS = glfw/build/src/libglfw3.a glew/lib/libGLEW.a glm/build/glm/libglm.a stb/build/stb_image.a imgui/libimgui.a
LDFLAGS += $(STATIC_LIBS)
LDFLAGS += -lGL -lGLX
CXXFLAGS += -DGLEW_STATIC

# Include directories.
INCLUDE_DIRS = glfw/include/ glew/include/ glu/include/ glm/ stb/include/ imgui/
CXXFLAGS += $(addprefix -I,$(INCLUDE_DIRS))

# Object files.
OBJS = Renderer.o Game.o log.o main.o

PROGRAM_NAME = engine

BUILD_DIR = build
BUILD_OBJS = $(addprefix $(BUILD_DIR)/,$(OBJS))
BUILD_DEPS = $(patsubst %.o,%.d,$(BUILD_OBJS))
-include $(BUILD_DEPS)

# Version info.
GIT_COMMIT := $(shell git describe --dirty --always)
CXXFLAGS += -DGIT_COMMIT=\"$(GIT_COMMIT)\"

ifdef DEBUG
CXXFLAGS += -O0 -g
else
CXXFLAGS += -O3 -DNDEBUG
LDFLAGS += -s
endif

# Disassembled program.
$(BUILD_DIR)/$(PROGRAM_NAME).s: $(BUILD_DIR)/$(PROGRAM_NAME)
	@mkdir -p $(dir $@)
	@echo "OBJDUMP $@"
	@objdump -drS $< > $@

# ELF-formatted program.
$(BUILD_DIR)/$(PROGRAM_NAME): $(BUILD_OBJS)
	@mkdir -p $(dir $@)
	@echo "CXXLD   $@"
	@g++ $(CXXFLAGS) $^ $(LDFLAGS) -o $@

# Make a .o from a .cc
$(BUILD_DIR)/%.o: src/%.cc
	@mkdir -p $(dir $@)
	@echo "CXX     $@"
	@g++ $(CXXFLAGS) -MMD -MP -c $< -o $@

# Default target - build the program and assembly.
all: $(BUILD_DIR)/$(PROGRAM_NAME) $(BUILD_DIR)/$(PROGRAM_NAME).s

# Play game.
play: $(BUILD_DIR)/$(PROGRAM_NAME)
	__GLX_VENDOR_LIBRARY_NAME=nvidia __NV_PRIME_RENDER_OFFLOAD=1 $(BUILD_DIR)/$(PROGRAM_NAME)

# Format all .cpp and .h files in the src directory.
format:
	find src -name "*.cc" -exec clang-format -i {} +;
	find src -name "*.h" -exec clang-format -i {} +;

# Remove built artifacts.
clean:
	rm -rf $(BUILD_DIR)

# List targets.
help:
	@echo "Available targets:"
	@echo "  all      - Build the program and assembly."
	@echo "  play     - Run the built program."
	@echo "  format   - Format all source files."
	@echo "  clean    - Remove built artifacts."

.PHONY: all play format clean help