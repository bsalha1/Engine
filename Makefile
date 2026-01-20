# Set build environment.
NTHREADS = $(shell nproc)
export CMAKE_BUILD_PARALLEL_LEVEL = $(NTHREADS)
export MAKEFLAGS = -j$(NTHREADS)

# Libraries.
LIBGLFW = glfw/build/src/libglfw3.a
LIBGLEW = glew/lib/libGLEW.a
LIBGLM = glm/build/glm/libglm.a
STB_IMAGE = stb/build/stb_image.a
LIBIMGUI = imgui/libimgui.a
LIBASSIMP = assimp/lib/libassimp.a

$(LIBGLFW):
	cd glfw && \
		cmake -S . -B build -D GLFW_BUILD_WAYLAND=OFF && \
		cd build && \
		make

$(LIBGLEW):
	cd glew/auto && \
		make && \
		cd .. && \
		make

$(LIBGLM):
	cd glm && \
		cmake \
			-DGLM_BUILD_TESTS=OFF \
			-DBUILD_SHARED_LIBS=OFF \
			-B build . && \
		cmake --build build -- all

$(STB_IMAGE):
	cd stb && \
		make

$(LIBIMGUI): 
	cd imgui && \
		g++ \
			-c \
			-I. \
			-I../glfw/include \
			backends/imgui_impl_opengl3.cpp \
			backends/imgui_impl_glfw.cpp \
			imgui_draw.cpp \
			imgui_tables.cpp \
			imgui_widgets.cpp \
			imgui.cpp \
			imgui_demo.cpp && \
		ar rcs libimgui.a *.o

$(LIBASSIMP):
	cd assimp && \
		cmake CMakeLists.txt -DBUILD_SHARED_LIBS=OFF -DASSIMP_BUILD_TESTS=OFF && \
		cmake --build .

STATIC_LIBS = $(LIBGLFW) $(LIBGLEW) $(LIBGLM) $(STB_IMAGE) $(LIBIMGUI) $(LIBASSIMP)
LDFLAGS += $(STATIC_LIBS)
LDFLAGS += -lGL -lGLX -lz -lminizip
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
CXXFLAGS += -DGIT_COMMIT=\"$(GIT_COMMIT)\"

# Disassembled file.
DISASSEMBLED_FILE = $(BUILD_DIR)/$(PROGRAM_NAME).s
$(DISASSEMBLED_FILE): $(BUILD_DIR)/$(PROGRAM_NAME)
	@mkdir -p $(dir $@)
	@echo "OBJDUMP $@"
	@objdump -drS $< > $@

# Debug flag disables optimizations, enables debug info and adds some debug targets.
ifdef DEBUG
CXXFLAGS += -O0 -g -rdynamic
LDFLAGS += -g -rdynamic
DEBUG_TARGETS = $(DISASSEMBLED_FILE)
else
CXXFLAGS += -O3 -DNDEBUG -Wall -Werror
LDFLAGS += -s
DEBUG_TARGETS =
endif

# No performance flag disables certain performance optimizations.
ifdef NO_PERF
CXXFLAGS += -DNPERF
endif

# Build directory.
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

# ELF-formatted program.
$(BUILD_DIR)/$(PROGRAM_NAME): $(BUILD_OBJS) $(CXXFLAGS_CACHE) $(LDFLAGS_CACHE) $(STATIC_LIBS)
	@mkdir -p $(dir $@)
	@echo "CXXLD   $@"
	@g++ $(CXXFLAGS) $(BUILD_OBJS) $(LDFLAGS) -o $@

# Make a .o from a .cc
$(BUILD_DIR)/%.o: src/%.cc $(CXXFLAGS_CACHE) $(LIBGLEW) $(LIBASSIMP)
	@mkdir -p $(dir $@)
	@echo "CXX     $@"
	@g++ $(CXXFLAGS) -MMD -MP -c $< -o $@

# Default target - build the program and debug targets, if any.
all: $(BUILD_DIR)/$(PROGRAM_NAME) $(DEBUG_TARGETS)

# Format all .cpp and .h files in the src directory.
format:
	find src -name "*.cc" -exec clang-format -i {} +;
	find src -name "*.h" -exec clang-format -i {} +;

# Remove built artifacts of this project only.
clean:
	rm -rf $(BUILD_DIR)

# Remove built artifacts including those of third party libraries.
clean_all: clean_local
	cd glfw && rm -rf build

	cd glew && make clean && rm -rf \
		auto/core/gl/EGL_VERSION_1_0 \
		auto/core/gl/EGL_VERSION_1_1 \
		auto/core/gl/EGL_VERSION_1_2 \
		auto/core/gl/EGL_VERSION_1_3 \
		auto/core/gl/EGL_VERSION_1_4 \
		auto/core/gl/EGL_VERSION_1_5 \
		auto/extensions \
		build/glew.rc \
		build/glewinfo.rc \
		build/visualinfo.rc \
		include \
		src/glew.c \
		src/glewinfo.c

	cd glm && rm -rf build

	cd stb && make clean

	cd imgui && rm -f *.o libimgui.a

	cd assimp && make clean ; rm -rf \
		CMakeCache.txt CMakeFiles Makefile assimp.pc cmake_install.cmake cmake_uninstall.cmake \
		code/CMakeFiles code/Makefile code/cmake_install.cmake \
		generated \
		include/assimp/config.h include/assimp/revision.h \
		lib

# List targets.
help:
	@echo "Available targets:"
	@echo "  all       - Build the executable."
	@echo "  format    - Format all source files."
	@echo "  clean     - Remove built artifacts."
	@echo "  clean_all - Remove built artifact and those of the third party libraries."

.PHONY: all format clean clean_all help FORCE