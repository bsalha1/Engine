# Set build environment.
NTHREADS = $(shell nproc)
export CMAKE_BUILD_PARALLEL_LEVEL = $(NTHREADS)
export MAKEFLAGS = -j$(NTHREADS)

# Set platform-specific variables.
ifeq ($(PLATFORM),windows)
CMAKE_TOOLCHAIN_FILE = $(shell realpath toolchains/mingw64.cmake)
CC = /usr/bin/x86_64-w64-mingw32-gcc
CXX = /usr/bin/x86_64-w64-mingw32-g++
AR = /usr/bin/x86_64-w64-mingw32-ar
LD = /usr/bin/x86_64-w64-mingw32-ld
INCLUDE_DIRS = /usr/x86_64-w64-mingw32/include

# Statically link libgcc and libstdc++ since there's no chance in hell the host
# has the same MinGW toolchain as the Docker container. This is apparently
# standard practice for Windows.
LDFLAGS = -lopengl32 -lgdi32 -static-libgcc -static-libstdc++

LIBASSIMP_CXXFLAGS = -Wno-error=maybe-uninitialized -Wno-error=array-bounds
LIBGLEW = glew/lib/libglew32.a
LIBGLEW_MAKE_FLAGS = SYSTEM=mingw
LIBZ = zlib/build/libzs.a
EXECUTABLE_EXTENSION = .exe
DEBUG_CXXFLAGS =
DEBUG_LDFLAGS =

else

CMAKE_TOOLCHAIN_FILE = 
CC = /usr/bin/gcc
CXX = /usr/bin/g++
AR = /usr/bin/ar
LD = /usr/bin/ld
INCLUDE_DIRS =
LDFLAGS = -lGL -lGLX -static-libgcc -static-libstdc++
LIBASSIMP_CXXFLAGS =
LIBGLEW = glew/lib/libGLEW.a
LIBGLEW_MAKE_FLAGS =
LIBZ = zlib/build/libz.a
EXECUTABLE_EXTENSION =
DEBUG_CXXFLAGS = -rdynamic
DEBUG_LDFLAGS = -rdynamic

endif

LIBGLFW = glfw/build/src/libglfw3.a
$(LIBGLFW):
	cd glfw && \
		cmake \
			-S . \
			-B build \
			-DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) \
			-DGLFW_BUILD_WAYLAND=OFF \
			-DGLFW_BUILD_EXAMPLES=OFF \
			-DGLFW_BUILD_TESTS=OFF && \
		cd build && \
		make

$(LIBGLEW):
	cd glew/auto && \
		make $(LIBGLEW_MAKE_FLAGS) CC=$(CC) LD=$(LD) AR=$(AR) && \
		cd .. && \
		make $(LIBGLEW_MAKE_FLAGS) CC=$(CC) LD=$(LD) AR=$(AR) glew.lib.static
CXXFLAGS += -DGLEW_STATIC

LIBGLM = glm/build/glm/libglm.a
$(LIBGLM):
	cd glm && \
		cmake . \
			-B build \
			-DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) \
			-DGLM_BUILD_TESTS=OFF \
			-DBUILD_SHARED_LIBS=OFF \ && \
		cmake --build build -- all

STB_IMAGE = stb/build/stb_image.a
$(STB_IMAGE):
	cd stb && \
		make CC=$(CC) AR=$(AR)

LIBIMGUI = imgui/libimgui.a
$(LIBIMGUI): 
	cd imgui && \
		$(CXX) \
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
		$(AR) rcs libimgui.a *.o

LIBASSIMP = assimp/lib/libassimp.a
$(LIBASSIMP):
	cd assimp && \
		cmake \
			-DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) \
			-DBUILD_SHARED_LIBS=OFF \
			-DASSIMP_BUILD_TESTS=OFF \
			-DASSIMP_BUILD_ASSIMP_TOOLS=OFF \
			-DASSIMP_BUILD_SAMPLES=OFF \
			-DASSIMP_BUILD_ZLIB=ON \
			-DCMAKE_CXX_FLAGS="$(LIBASSIMP_CXXFLAGS)" \
			CMakeLists.txt && \
		cmake --build .

$(LIBZ):
	cd zlib && \
		cmake . \
			-B build \
			-DCMAKE_TOOLCHAIN_FILE=$(CMAKE_TOOLCHAIN_FILE) \
			-DBUILD_SHARED_LIBS=OFF && \
		cmake --build build

# Add static libraries to linker flags.
STATIC_LIBS = $(LIBGLFW) $(LIBGLEW) $(LIBGLM) $(STB_IMAGE) $(LIBIMGUI) $(LIBASSIMP) $(LIBZ)
LDFLAGS := $(STATIC_LIBS) $(LDFLAGS)

# Include directories.
INCLUDE_DIRS += glfw/include/ glew/include/ glu/include/ glm/ stb/include/ imgui/ assimp/include/
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

# Debug flag disables optimizations and enables debug info.
ifdef DEBUG
CXXFLAGS += -O3 -g $(DEBUG_CXXFLAGS)
LDFLAGS += -g $(DEBUG_LDFLAGS)  
else
CXXFLAGS += -O0 -DNDEBUG -Wall -Werror
LDFLAGS += -s
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

# Executable.
EXECUTABLE = $(BUILD_DIR)/$(PROGRAM_NAME)$(EXECUTABLE_EXTENSION)
$(EXECUTABLE): $(BUILD_OBJS) $(CXXFLAGS_CACHE) $(LDFLAGS_CACHE) $(STATIC_LIBS)
	@mkdir -p $(dir $@)
	@echo "CXXLD   $@"
	@$(CXX) $(CXXFLAGS) $(BUILD_OBJS) $(LDFLAGS) -o $@

# Make a .o from a .cc
$(BUILD_DIR)/%.o: src/%.cc $(CXXFLAGS_CACHE) $(LIBGLEW) $(LIBASSIMP)
	@mkdir -p $(dir $@)
	@echo "CXX     $@"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Default target - build the executable.
all: $(EXECUTABLE)

# Format all .cpp and .h files in the src directory.
format:
	find src -name "*.cc" -exec clang-format -i {} +;
	find src -name "*.h" -exec clang-format -i {} +;

# Remove built artifacts of this project only.
clean:
	rm -rf $(BUILD_DIR)

# Remove built artifacts including those of third party libraries.
clean_all: clean
	cd assimp && git clean -fxd
	cd imgui && git clean -fxd
	cd glew && git clean -fxd
	cd glfw && git clean -fxd
	cd glm && git clean -fxd
	cd stb && make clean
	cd zlib && git clean -fxd

# List targets.
help:
	@echo "Available targets:"
	@echo "  all       - Build the executable."
	@echo "  format    - Format all source files."
	@echo "  clean     - Remove built artifacts."
	@echo "  clean_all - Remove built artifact and those of the third party libraries."

.PHONY: all format clean clean_all help FORCE