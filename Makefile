# Cross-platform Makefile for relay-c (Windows MinGW + Linux)

CC ?= gcc
MINGW ?= C:/msys64/mingw64
PKG_CONFIG ?= pkg-config

OUTDIR ?= output
BUILDDIR := $(OUTDIR)/build
DISTDIR := $(OUTDIR)/relay-c

CFLAGS_COMMON := -Wall -Wno-unused-parameter -D_WIN32_WINNT=0x0601 \
	-fstack-protector-strong -Iinclude -Ithird_party

CFLAGS ?= -O2 $(CFLAGS_COMMON)

ifeq ($(OS),Windows_NT)
  CFLAGS += -I$(MINGW)/include
endif

CJSON_CFLAGS := $(shell $(PKG_CONFIG) --cflags libcjson 2>/dev/null)
CJSON_LIBS   := $(shell $(PKG_CONFIG) --libs libcjson 2>/dev/null)
ifeq ($(strip $(CJSON_LIBS)),)
CJSON_LIBS := -lcjson
endif

CFLAGS += $(CJSON_CFLAGS)
ifeq ($(OS),Windows_NT)
  LIBS := $(CJSON_LIBS) -lws2_32 -lpthread
  APP := $(BUILDDIR)/relay-c.exe
  TEST := $(BUILDDIR)/test_relay.exe
  EXE_NAME := relay-c.exe
else
  LIBS := $(CJSON_LIBS) -lpthread
  APP := $(BUILDDIR)/relay-c
  TEST := $(BUILDDIR)/test_relay
  EXE_NAME := relay-c
endif

SRC := src/main.c src/util.c src/action.c src/protocol.c src/serial.c src/config.c \
       src/service.c src/relay_lock.c src/relay_startup.c src/ports.c src/http_api.c \
       src/http_pages.c src/http_control_page.c src/http_docs_page.c \
       src/http_config_page.c src/http_server.c src/eadk_discovery.c src/audit.c \
       src/cli_http.c third_party/mongoose.c
OBJ := $(addprefix $(BUILDDIR)/,$(SRC:.c=.o))

DESKTOP := $(BUILDDIR)/relay_desktop.exe
WEBVIEW2_DLL := ../ssh-bridge-c/third_party/webview2/x64/WebView2Loader.dll

.PHONY: all test clean dirs desktop package

all: dirs $(APP)

desktop:
	$(MAKE) -C windows_desktop OUTDIR=$(abspath $(OUTDIR)) MINGW=$(MINGW)

dirs:
	@mkdir -p $(BUILDDIR)/src $(BUILDDIR)/third_party $(BUILDDIR)/tests $(DISTDIR) $(OUTDIR)

$(APP): $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

$(BUILDDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

TEST_SRC := tests/test_relay.c src/util.c src/action.c src/protocol.c src/serial.c \
            src/config.c src/service.c src/relay_lock.c
TEST_OBJ := $(addprefix $(BUILDDIR)/,$(TEST_SRC:.c=.o))

$(TEST): $(TEST_OBJ) | dirs
	$(CC) -o $@ $(TEST_OBJ) $(LIBS)

test: $(TEST)
	$(TEST)

ifeq ($(OS),Windows_NT)
package: $(APP) desktop | dirs
else
package: $(APP) | dirs
endif
	@echo "Packaging $(DISTDIR) ..."
	rm -rf "$(DISTDIR)"
	mkdir -p "$(DISTDIR)"
	cp -f "$(APP)" "$(DISTDIR)/$(EXE_NAME)"
	cp -f boards.json.example "$(DISTDIR)/boards.json.example"
	@if [ ! -f "$(DISTDIR)/boards.json" ]; then cp -f boards.json.example "$(DISTDIR)/boards.json"; fi
	cp -f README.md "$(DISTDIR)/README.txt" 2>/dev/null || true
ifeq ($(OS),Windows_NT)
	@test -f "$(DESKTOP)" || (echo "ERROR: missing $(DESKTOP)"; exit 1)
	cp -f "$(DESKTOP)" "$(DISTDIR)/relay_desktop.exe"
	cp -f "$(MINGW)/bin/libcjson-1.dll" "$(DISTDIR)/" 2>/dev/null || true
	cp -f "$(MINGW)/bin/libwinpthread-1.dll" "$(DISTDIR)/" 2>/dev/null || true
	@if [ -f "$(WEBVIEW2_DLL)" ]; then \
		cp -f "$(WEBVIEW2_DLL)" "$(DISTDIR)/WebView2Loader.dll"; \
	fi
	cp -f tools/dist_run.bat "$(DISTDIR)/run.bat"
else
	cp -f tools/dist_run.sh "$(DISTDIR)/run.sh" 2>/dev/null || true
	chmod +x "$(DISTDIR)/$(EXE_NAME)" "$(DISTDIR)/run.sh" 2>/dev/null || true
endif
	@echo "OK: $(DISTDIR)"

clean:
	-rm -rf $(OUTDIR)
	-$(MAKE) -C windows_desktop OUTDIR=$(abspath $(OUTDIR)) clean 2>/dev/null || true
