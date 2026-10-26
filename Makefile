.DEFAULT_GOAL := all

PROJECT = foilcard
RAYLIB_PATH = raylib
RAYLIB_REPO = https://github.com/raysan5/raylib.git
RAYLIB_TAG = 5.5
RAYLIB_SRC = $(RAYLIB_PATH)/src
RAYLIB_BUILD = $(RAYLIB_PATH)/build

ifeq ($(OS),Windows_NT)
TARGETS = $(PROJECT).exe
WIN_CC = gcc
WIN_AR = ar
WIN_LIB = $(RAYLIB_SRC)
WIN_RELEASE = .
else
TARGETS = $(PROJECT).exe $(PROJECT)
WIN_CC = x86_64-w64-mingw32-gcc
WIN_AR = x86_64-w64-mingw32-ar
WIN_LIB = $(RAYLIB_BUILD)/windows
WIN_RELEASE = ../build/windows
endif

CFLAGS = -std=c99 -O2 -Wall -Wextra -Isrc -I$(RAYLIB_SRC)

WIN_LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm -lm -static-libgcc

LINUX_CC = gcc
LINUX_LIB = $(RAYLIB_BUILD)/linux
LINUX_RELEASE = ../build/linux
LINUX_LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

SOURCES = $(wildcard src/*.c src/*.h)
UNITS = $(wildcard src/*.c)

ifeq ($(shell echo "test"),"test")
winpath = $(subst /,\,$(1))
MKDIR = if not exist $(call winpath,$(1)) mkdir $(call winpath,$(1))
RM_FILES = -del /q /f $(call winpath,$(1)) 2>nul
RM_TREE = -if exist $(call winpath,$(1)) rmdir /s /q $(call winpath,$(1))
else
MKDIR = mkdir -p $(1)
RM_FILES = rm -f $(1)
RM_TREE = rm -rf $(1)
endif

.PHONY: all windows linux clean distclean re

all: $(TARGETS)

windows: $(PROJECT).exe

linux: $(PROJECT)

$(RAYLIB_SRC)/raylib.h:
	git clone --depth 1 --branch $(RAYLIB_TAG) "$(RAYLIB_REPO)" "$(RAYLIB_PATH)"

$(WIN_LIB)/libraylib.a: | $(RAYLIB_SRC)/raylib.h
	$(call MKDIR,$(WIN_LIB))
	$(MAKE) -C $(RAYLIB_SRC) clean PLATFORM=PLATFORM_DESKTOP PLATFORM_OS=WINDOWS CC=$(WIN_CC) AR=$(WIN_AR)
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP PLATFORM_OS=WINDOWS CC=$(WIN_CC) AR=$(WIN_AR) RAYLIB_LIBTYPE=STATIC RAYLIB_RELEASE_PATH=$(WIN_RELEASE)

$(LINUX_LIB)/libraylib.a: | $(RAYLIB_SRC)/raylib.h
	$(call MKDIR,$(LINUX_LIB))
	$(MAKE) -C $(RAYLIB_SRC) clean PLATFORM=PLATFORM_DESKTOP PLATFORM_OS=LINUX CC=$(LINUX_CC)
	$(MAKE) -C $(RAYLIB_SRC) PLATFORM=PLATFORM_DESKTOP PLATFORM_OS=LINUX CC=$(LINUX_CC) RAYLIB_LIBTYPE=STATIC RAYLIB_RELEASE_PATH=$(LINUX_RELEASE)

$(PROJECT).exe: $(SOURCES) $(WIN_LIB)/libraylib.a
	$(WIN_CC) $(CFLAGS) $(UNITS) -o $@ -L$(WIN_LIB) $(WIN_LIBS)

$(PROJECT): $(SOURCES) $(LINUX_LIB)/libraylib.a
	$(LINUX_CC) $(CFLAGS) $(UNITS) -o $@ -L$(LINUX_LIB) $(LINUX_LIBS)

clean:
	$(call RM_FILES,$(PROJECT) $(PROJECT).exe)

distclean: clean
	$(call RM_TREE,$(RAYLIB_PATH))

re: clean
	$(MAKE) all
