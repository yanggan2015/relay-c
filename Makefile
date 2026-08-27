# MSYS2 mingw64 Makefile for relay-c (Windows cmd + Unix compatible)

CC ?= gcc
MINGW ?= C:/msys64/mingw64
PKG_CONFIG ?= pkg-config

OUTDIR ?= output
BUILDDIR := $(OUTDIR)/build

CFLAGS_COMMON := -Wall -Wno-unused-parameter -D_WIN32_WINNT=0x0601 \
	-fstack-protector-strong -Iinclude -Ithird_party -I$(MINGW)/include

CFLAGS ?= -O2 $(CFLAGS_COMMON)

CJSON_CFLAGS := $(shell $(PKG_CONFIG) --cflags libcjson 2>/dev/null)
CJSON_LIBS   := $(shell $(PKG_CONFIG) --libs libcjson 2>/dev/null)
ifeq ($(strip $(CJSON_LIBS)),)
CJSON_LIBS := -lcjson
endif

CFLAGS += $(CJSON_CFLAGS)
LIBS := $(CJSON_LIBS) -lws2_32 -lpthread

SRC := src/main.c src/util.c src/action.c src/protocol.c src/serial.c src/config.c \
       src/service.c src/relay_lock.c src/relay_startup.c src/ports.c src/http_api.c \
       src/http_pages.c src/http_control_page.c src/http_docs_page.c \
       src/http_config_page.c src/http_server.c third_party/mongoose.c
OBJ := $(addprefix $(BUILDDIR)/,$(SRC:.c=.o))

APP := $(BUILDDIR)/relay-c.exe
TEST := $(BUILDDIR)/test_relay.exe

.PHONY: all test clean dirs

ifeq ($(OS),Windows_NT)
SHELL := cmd
MKDIR = if not exist "$(subst /,\,$(1))" mkdir "$(subst /,\,$(1))"
else
MKDIR = mkdir -p "$(1)"
endif

all: dirs $(APP)

dirs:
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(BUILDDIR)\src)" mkdir "$(subst /,\,$(BUILDDIR)\src)"
	@if not exist "$(subst /,\,$(BUILDDIR)\third_party)" mkdir "$(subst /,\,$(BUILDDIR)\third_party)"
	@if not exist "$(subst /,\,$(OUTDIR))" mkdir "$(subst /,\,$(OUTDIR))"
else
	@mkdir -p $(BUILDDIR)/src $(BUILDDIR)/third_party $(OUTDIR)
endif

$(APP): $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

$(BUILDDIR)/%.o: %.c
ifeq ($(OS),Windows_NT)
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
else
	@mkdir -p $(dir $@)
endif
	$(CC) $(CFLAGS) -c -o $@ $<

TEST_SRC := tests/test_relay.c src/util.c src/action.c src/protocol.c src/serial.c \
            src/config.c src/service.c src/relay_lock.c
TEST_OBJ := $(addprefix $(BUILDDIR)/,$(TEST_SRC:.c=.o))

$(TEST): $(TEST_OBJ) | dirs
	$(CC) -o $@ $(TEST_OBJ) $(LIBS)

test: $(TEST)
ifeq ($(OS),Windows_NT)
	@$(TEST)
else
	@./$(TEST)
endif

clean:
ifeq ($(OS),Windows_NT)
	@if exist "$(subst /,\,$(OUTDIR))" rmdir /s /q "$(subst /,\,$(OUTDIR))"
else
	@rm -rf $(OUTDIR)
endif
