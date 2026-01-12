# Libraries.
STATIC_LIBS = glfw/build/src/libglfw3.a glew/lib/libGLEW.a glm/build/glm/libglm.a stb/build/stb_image.a imgui/libimgui.a assimp/lib/libassimp.a
LDFLAGS += $(STATIC_LIBS)
LDFLAGS += -lGL -lGLX -lz
CXXFLAGS += -DGLEW_STATIC

# Include directories.
INCLUDE_DIRS = glfw/include/ glew/include/ glu/include/ glm/ stb/include/ imgui/ assimp/include/
CXXFLAGS += $(addprefix -I,$(INCLUDE_DIRS))

# Object files.
OBJS = IndexBuffer.o FramebufferTexture.o CubemapTexture.o Texture.o Model.o VertexArray.o \
	DebugMenu.o PauseMenu.o SettingsMenu.o ConfirmMenu.o MenuManager.o \
	assert_util.o \
	ShaderLoader.o Shader.o \
	Renderer.o Game.o log.o main.o

PROGRAM_NAME = engine

BUILD_DIR = build
BUILD_OBJS = $(addprefix $(BUILD_DIR)/,$(OBJS))
BUILD_DEPS = $(patsubst %.o,%.d,$(BUILD_OBJS))
-include $(BUILD_DEPS)

# Version info.
GIT_COMMIT := $(shell git describe --dirty --always)
CXXFLAGS += -DGIT_COMMIT=\"$(GIT_COMMIT)\"

# Debug flag disables optimizations and enables debug info.
ifdef DEBUG
CXXFLAGS += -O0 -g -rdynamic
LDFLAGS += -g -rdynamic
else
CXXFLAGS += -O3 -DNDEBUG -Wall -Werror
LDFLAGS += -s
endif

# No performance flag disables certain performance optimizations.
ifdef NO_PERF
CXXFLAGS += -DNPERF
endif

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# A dummy target which always runs.
FORCE:

# Create flag cache files to know to rebuild when flags change.
define make_flag_cache
$1_CACHE := $(BUILD_DIR)/.$1_cache

$$($1_CACHE): FORCE $(BUILD_DIR)
	@echo '$$($1)' | cmp -s - $$@ || echo '$$($1)' > $$@
endef
$(eval $(call make_flag_cache,CXXFLAGS))
$(eval $(call make_flag_cache,LDFLAGS))

# Disassembled program.
$(BUILD_DIR)/$(PROGRAM_NAME).s: $(BUILD_DIR)/$(PROGRAM_NAME)
	@mkdir -p $(dir $@)
	@echo "OBJDUMP $@"
	@objdump -drS $< > $@

# ELF-formatted program.
$(BUILD_DIR)/$(PROGRAM_NAME): $(BUILD_OBJS) $(CXXFLAGS_CACHE) $(LDFLAGS_CACHE)
	@mkdir -p $(dir $@)
	@echo "CXXLD   $@"
	@g++ $(CXXFLAGS) $(BUILD_OBJS) $(LDFLAGS) -o $@

# Make a .o from a .cc
$(BUILD_DIR)/%.o: src/%.cc $(CXXFLAGS_CACHE)
	@mkdir -p $(dir $@)
	@echo "CXX     $@"
	@g++ $(CXXFLAGS) -MMD -MP -c $< -o $@

# Default target - build the program and assembly.
all: $(BUILD_DIR)/$(PROGRAM_NAME) $(BUILD_DIR)/$(PROGRAM_NAME).s

# Play game.
play: $(BUILD_DIR)/$(PROGRAM_NAME)
	$(BUILD_DIR)/$(PROGRAM_NAME)

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
	@echo "  play     - Build and run the program."
	@echo "  format   - Format all source files."
	@echo "  clean    - Remove built artifacts."

.PHONY: all play format clean help FORCE