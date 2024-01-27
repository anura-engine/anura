#
# Main Makefile, intended for use on Linux/X11 and compatible platforms
# using GNU Make.
#
# It should guess the paths to the game dependencies on its own, except for
# Boost which is assumed to be installed to the default locations. If you have
# installed Boost to a non-standard location, you will need to override CXXFLAGS
# and LDFLAGS with any applicable -I and -L arguments.
#
# The main options are:
#
#   CCACHE           The ccache binary that should be used when USE_CCACHE is
#                     enabled (see below). Defaults to 'ccache'.
#   CXX              C++ compiler comand line.
#   CXXFLAGS         Additional C++ compiler options.
#   OPTIMIZE         If set to 'yes' (default), builds with compiler
#                     optimizations enabled (-O2). You may alternatively use
#                     CXXFLAGS to set your own optimization options.
#   LDFLAGS          Additional linker options.
#   USE_CCACHE       If set to 'yes' (default), builds using the CCACHE binary
#                     to run the compiler. If ccache is not installed (i.e.
#                     found in PATH), this option has no effect.
#   USE_DB_CLIENT    If set to 'yes' and couchbase is present will build.
#

OPTIMIZE?=yes

CCACHE?=ccache
USE_CCACHE?=$(shell which $(CCACHE) > /dev/null 2>&1 && echo yes)
ifneq ($(USE_CCACHE),yes)
CCACHE=
USE_CCACHE=no
endif

SANITIZE_ADDRESS?=
ifneq ($(SANITIZE_ADDRESS), yes)
SANITIZE_ADDRESS=no
endif

SANITIZE_UNDEFINED?=
ifneq ($(SANITIZE_UNDEFINED), yes)
SANITIZE_UNDEFINED=no
endif

ifeq ($(OPTIMIZE),yes)
BASE_CXXFLAGS += -O2
endif

BASE_CXXFLAGS += -Wall -Werror

ifneq (,$(findstring clang, `$(CXX)`))
SANITIZE_UNDEFINED=
BASE_CXXFLAGS += -Qunused-arguments -Wno-unknown-warning-option -Wno-deprecated-register
else ifneq (, $(findstring g++, `$(CXX)`))
BASE_CXXFLAGS += -Wno-literal-suffix -Wno-sign-compare -fdiagnostics-color=auto
endif

SDL2_CONFIG?=sdl2-config
USE_SDL2?=$(shell which $(SDL2_CONFIG) 2>&1 > /dev/null && echo yes)

ifneq ($(USE_SDL2),yes)
$(error SDL2 not found, SDL-1.2 is no longer supported)
endif

BASE_CXXFLAGS += $(shell $(SDL2_CONFIG) --cflags)
LDFLAGS+ = $(shell $(SDL2_CONFIG) --ldflags)

TARBALL := /var/www/anura/anura-$(shell date +"%Y%m%d-%H%M").tar.bz2

# Initial compiler options, used before CXXFLAGS and CPPFLAGS. -rdynamic -Wno-literal-suffix
# Notes:
#   - DBOOST_BIND_GLOBAL_PLACEHOLDERS needed for our liberal use of _1, _2, etc.
#   - Wno-deprecated-declarations because boost uses std::auto_ptr in a few places.
#   - I for vcpgk to find stuff.
BASE_CXXFLAGS += -std=c++0x -g -fno-inline-functions \
	-fthreadsafe-statics \
	-Wno-narrowing -Wno-reorder -Wno-unused \
	-Wno-unknown-pragmas -Wno-overloaded-virtual \
	-DBOOST_BIND_GLOBAL_PLACEHOLDERS \
	-Wno-deprecated-declarations \
	-I./vcpkg_installed/x64-linux/include

LDFLAGS?=-rdynamic
LDFLAGS += -L./vcpkg_installed/x64-linux/lib

PKG_CONFIG_PATH:=$(shell pwd)/vcpkg_installed/x64-linux/lib/pkgconfig:$(shell pwd)/vcpkg_installed/x64-linux/share/pkgconfig:$(PKG_CONFIG_PATH)
MANDATORY_LIBS=sdl2 glew SDL2_image SDL2_ttf libpng zlib freetype2 cairo

# Check for sanitize-address option
ifeq ($(SANITIZE_ADDRESS), yes)
BASE_CXXFLAGS += -g3 -fsanitize=address
LDFLAGS += -fsanitize=address
endif

# Check for sanitize-undefined option
ifeq ($(SANITIZE_UNDEFINED), yes)
BASE_CXXFLAGS += -fsanitize=undefined
endif

# Compiler include options, used after CXXFLAGS and CPPFLAGS.
INC := $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags $(MANDATORY_LIBS))

ifdef STEAM_RUNTIME_ROOT
	INC += -isystem $(STEAM_RUNTIME_ROOT)/include
endif

# Linker library options. (needs gl?)
LIBS := -lvorbisfile \
	$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs x11 gl ) \
	-logg -lvorbis \
	$(shell PKG_CONFIG_PATH= pkg-config --libs sdl2 SDL2_image SDL2_ttf) \
	$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs glew libpng zlib freetype2 cairo) \
	 -logg -lrt -lstdc++ -lm -lX11 -lboost_system -lboost_locale -lboost_thread -lXext -lX11 -lSDL2

# libvpx check
USE_LIBVPX?=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --exists vpx && echo yes)
ifeq ($(USE_LIBVPX),yes)
	BASE_CXXFLAGS += -DUSE_LIBVPX
	INC += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags vpx)
	LIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs vpx)
else
USE_LIBVPX=no
endif

# couchbase check
USE_DB_CLIENT?=no
ifeq ($(USE_DB_CLIENT),yes)
    BASE_CXXFLAGS += -DUSE_DBCLIENT
    LIBS += -lcouchbase
else
USE_DB_CLIENT=no
endif

# cairo check
USE_SVG?=$(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --exists cairo && echo yes)
ifeq ($(USE_SVG),yes)
	BASE_CXXFLAGS += -DUSE_SVG
	INC += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --cflags cairo)
	LIBS += $(shell PKG_CONFIG_PATH=$(PKG_CONFIG_PATH) pkg-config --libs cairo)
else
USE_SVG=no
endif

MODULES   := kre svg tiled hex xhtml imgui_additions

SRC_DIR   := $(addprefix src/,$(MODULES)) src
BUILD_DIR := $(addprefix build/,$(MODULES)) build

SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
OBJ       := $(patsubst src/%.cpp,./build/%.o,$(SRC))
DEPS      := $(patsubst src/%.cpp,./build/%.d,$(SRC))
INCLUDES  := $(addprefix -I,$(SRC_DIR))

CPPFLAGS += -DIMGUI_USER_CONFIG=\"$(abspath src/imgui_additions/imconfig.h)\"

INC += -Iimgui

SRC += imgui/imgui.cpp
SRC += imgui/imgui_draw.cpp
SRC += imgui/imgui_tables.cpp
SRC += imgui/imgui_widgets.cpp

OBJ += imgui/imgui.o
OBJ += imgui/imgui_draw.o
OBJ += imgui/imgui_tables.o
OBJ += imgui/imgui_widgets.o

SRC_DIR += imgui
BUILD_DIR += imgui

vpath %.cpp $(SRC_DIR)

CPPFLAGS += -MMD -MP
define cc-command
$1/%.o: %.cpp
	@echo "Building:" $$<
	@$(CCACHE) $(CXX) $(CPPFLAGS) $(BASE_CXXFLAGS) $(CXXFLAGS) $(INC) $(INCLUDES) -MF $$@.d -c -o $$@ $$<
endef

.PHONY: all checkdirs clean

all: checkdirs anura

anura: $(OBJ)
	@echo "Linking : anura"
	@$(CXX) \
		$(BASE_CXXFLAGS) $(LDFLAGS) $(CXXFLAGS) $(CPPFLAGS) \
		$(OBJ) -o anura \
		$(LIBS) -lboost_regex -lboost_system -lboost_filesystem -lboost_locale -licui18n -licuuc -licudata -lpthread -fthreadsafe-statics

checkdirs: $(BUILD_DIR)
	@printf "\
	OPTIMIZE            : $(OPTIMIZE)\n\
USE_CCACHE          : $(USE_CCACHE)\n\
	CCACHE              : $(CCACHE)\n\
SANITIZE_ADDRESS    : $(SANITIZE_ADDRESS)\n\
SANITIZE_UNDEFINED  : $(SANITIZE_UNDEFINED)\n\
	USE_DB_CLIENT       : $(USE_DB_CLIENT)\n\
USE_LIBVPX          : $(USE_LIBVPX)\n\
	USE_SDL2            : $(USE_SDL2)\n\
CXX                 : $(CXX)\n\
	BASE_CXXFLAGS       : $(BASE_CXXFLAGS)\n\
	CXXFLAGS            : $(CXXFLAGS)\n\
LDFLAGS             : $(LDFLAGS)\n\
INC                 : $(INC)\n\
LIBS                : $(LIBS)\n\
PKG_CONFIG_PATH     : $(PKG_CONFIG_PATH)\n"


$(BUILD_DIR):
	@mkdir -p $@

clean:
	rm -f $(foreach bdir,$(BUILD_DIR),$(bdir)/*.o) $(foreach bdir,$(BUILD_DIR),$(bdir)/*.o.d) anura

unittests: anura
	./anura --tests

tarball: unittests
	@strip anura
	@tar --transform='s,^,anura/,g' -cjf $(TARBALL) anura data/ images/
	@cp $(TARBALL) /var/www/anura/anura-latest-linux.tar.bz2

$(foreach bdir,$(BUILD_DIR),$(eval $(call cc-command,$(bdir))))

# pull in dependency info for *existing* .o files
-include $(OBJ:.o=.o.d)
