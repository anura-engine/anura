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
USE_LUA?=yes
USE_BOX2D?=yes
CCACHE?=ccache
USE_CCACHE?=$(shell which $(CCACHE) 2>&1 > /dev/null && echo yes)
ifneq ($(USE_CCACHE),yes)
CCACHE=
endif

SANITIZE_ADDRESS?=
ifneq ($(SANITIZE_ADDRESS), yes)
SANITIZE_ADDRESS=
endif

SANITIZE_UNDEFINED?=yes
ifneq ($(SANITIZE_UNDEFINED), yes)
SANITIZE_UNDEFINED=
endif

ifeq ($(OPTIMIZE),yes)
BASE_CXXFLAGS += -O2
endif

ifneq ($(USE_LUA), yes)
USE_LUA=
endif

BASE_CXXFLAGS += -Wall -Werror

ifneq (,$(findstring clang, `$(CXX)`))
SANITIZE_UNDEFINED=
BASE_CXXFLAGS += -Qunused-arguments -Wno-unknown-warning-option -Wno-deprecated-register
ifeq ($(USE_LUA), yes)
BASE_CXXFLAGS += -Wno-pointer-bool-conversion -Wno-parentheses-equality
endif
else ifneq (, $(findstring g++, `$(CXX)`))
GCC_GTEQ_490 := $(shell expr `$(CXX) -dumpversion | sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/'` \>= 40900)
BASE_CXXFLAGS += -Wno-literal-suffix -Wno-sign-compare
ifeq "$(GCC_GTEQ_490)" "1"
BASE_CXXFLAGS += -fdiagnostics-color=auto
else
SANITIZE_UNDEFINED=
endif
endif

SDL2_CONFIG?=sdl2-config
USE_SDL2?=$(shell which $(SDL2_CONFIG) 2>&1 > /dev/null && echo yes)

ifneq ($(USE_SDL2),yes)
$(error SDL2 not found, SDL-1.2 is no longer supported)
endif

ifeq ($(USE_LUA), yes)
BASE_CXXFLAGS += -DUSE_LUA
#USE_LUA := yes # ?=$(shell pkg-config --exists lua5.2 && echo yes)
endif

TARBALL := /var/www/anura/anura-$(shell date +"%Y%m%d-%H%M").tar.bz2

# Initial compiler options, used before CXXFLAGS and CPPFLAGS. -rdynamic -Wno-literal-suffix
BASE_CXXFLAGS += -std=c++0x -g -fno-inline-functions \
	-fthreadsafe-statics \
	-Wno-narrowing -Wno-reorder -Wno-unused \
	-Wno-unknown-pragmas -Wno-overloaded-virtual

LDFLAGS?=-rdynamic

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
INC := -isystem external/header-only-libs $(shell pkg-config --cflags x11 sdl2 glew SDL2_image SDL2_ttf libpng zlib freetype2 cairo)

ifdef STEAM_RUNTIME_ROOT
	INC += -isystem $(STEAM_RUNTIME_ROOT)/include
endif

# Linker library options.
LIBS := $(shell pkg-config --libs x11 gl ) \
	$(shell pkg-config --libs sdl2 glew SDL2_image libpng zlib freetype2 cairo) \
	-lSDL2_ttf -lSDL2_mixer -lrt

# libvpx check
USE_LIBVPX?=$(shell pkg-config --exists vpx && echo yes)
ifeq ($(USE_LIBVPX),yes)
	BASE_CXXFLAGS += -DUSE_LIBVPX
	INC += $(shell pkg-config --cflags vpx)
	LIBS += $(shell pkg-config --libs vpx)
endif

# couchbase check
USE_DB_CLIENT?=no
ifeq ($(USE_DB_CLIENT),yes)
    BASE_CXXFLAGS += -DUSE_DBCLIENT
    LIBS += -lcouchbase
endif

# cairo check
USE_SVG?=$(shell pkg-config --exists cairo && echo yes)
ifeq ($(USE_SVG),yes)
	BASE_CXXFLAGS += -DUSE_SVG
	INC += $(shell pkg-config --cflags cairo)
	LIBS += $(shell pkg-config --libs cairo)
endif

MODULES   := kre svg Box2D tiled hex xhtml
ifeq ($(USE_LUA),yes)
MODULES += eris
endif

SRC_DIR   := $(addprefix src/,$(MODULES)) src
BUILD_DIR := $(addprefix build/,$(MODULES)) build

SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
OBJ       := $(patsubst src/%.cpp,./build/%.o,$(SRC))
DEPS      := $(patsubst src/%.cpp,./build/%.o.d,$(SRC))
INCLUDES  := $(addprefix -I,$(SRC_DIR))

vpath %.cpp $(SRC_DIR)

CPPFLAGS += -MMD -MP
define cc-command
$1/%.o: %.cpp
	@echo "Building:" $$<
	@$(CCACHE) $(CXX) $(BASE_CXXFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(INC) $(INCLUDES) -c -o $$@ $$<
endef

.PHONY: all checkdirs clean

all: checkdirs anura

anura: $(OBJ)
	@echo "Linking : anura"
	@$(CXX) \
		$(BASE_CXXFLAGS) $(LDFLAGS) $(CXXFLAGS) $(CPPFLAGS) \
		$(OBJ) -o anura \
		$(LIBS) -lboost_regex -lboost_system -lboost_filesystem -lpthread -fthreadsafe-statics

checkdirs: $(BUILD_DIR)
	@echo -e \
	 " OPTIMIZE            : $(OPTIMIZE)\n" \
	  "USE_CCACHE          : $(USE_CCACHE)\n" \
	  "CCACHE              : $(CCACHE)\n" \
	  "SANITIZE_ADDRESS    : $(SANITIZE_ADDRESS)\n" \
	  "SANITIZE_UNDEFINED  : $(SANITIZE_UNDEFINED)\n" \
	  "USE_DB_CLIENT       : $(USE_DB_CLIENT)\n" \
	  "USE_BOX2D           : $(USE_BOX2D)\n" \
	  "USE_LIBVPX          : $(USE_LIBVPX)\n" \
	  "USE_LUA             : $(USE_LUA)\n" \
	  "USE_SDL2            : $(USE_SDL2)\n" \
	  "CXX                 : $(CXX)\n" \
	  "BASE_CXXFLAGS       : $(BASE_CXXFLAGS)\n" \
	  "CXXFLAGS            : $(CXXFLAGS)\n" \
	  "LDFLAGS             : $(LDFLAGS)\n" \
	  "LIBS                : $(LIBS)"


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
