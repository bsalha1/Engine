# Libraries.
STATIC_LIBS = glfw/build/src/libglfw3.a glew/lib/libGLEW.a
LDFLAGS += $(STATIC_LIBS)
LDFLAGS += -lGL -lGLX

# Include directories.
INCLUDE_DIRS = glfw/include glew/include glu/include
CXXFLAGS += $(addprefix -I,$(INCLUDE_DIRS))

# Object files.
OBJS = Game.o log.o main.o

PROGRAM_NAME = engine

BUILD_DIR = build
BUILD_OBJS = $(addprefix $(BUILD_DIR)/,$(OBJS))
BUILD_DEPS = $(patsubst %.o,%.d,$(BUILD_OBJS))
-include $(BUILD_DEPS)

LINKER_SCRIPT = $(PROGRAM_NAME).ld

# Version info.
GIT_COMMIT := $(shell git describe --dirty --always)
CXXFLAGS += -DGIT_COMMIT=\"$(GIT_COMMIT)\"

# Disassembled program.
$(BUILD_DIR)/$(PROGRAM_NAME).s: $(BUILD_DIR)/$(PROGRAM_NAME)
	@mkdir -p $(dir $@)
	@echo "OBJDUMP $@"
	#@$(OBJDUMP) -drS $< > $@

# ELF-formatted program.
$(BUILD_DIR)/$(PROGRAM_NAME): $(BUILD_OBJS)
	@mkdir -p $(dir $@)
	@echo "CXXLD   $@"
	@$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

# Make a .o from a .cc
$(BUILD_DIR)/%.o: src/%.cc
	@mkdir -p $(dir $@)
	@echo "CXX     $@"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Default target - build the program and assembly.
all: $(BUILD_DIR)/$(PROGRAM_NAME) $(BUILD_DIR)/$(PROGRAM_NAME).s

format:
	find src -name "*.cc" -exec clang-format-14 -i {} +;
	find src -name "*.h" -exec clang-format-14 -i {} +;

# Remove built artifacts.
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all format clean