# MSYS2 mingw64 Makefile for relay-c
# Intermediate artifacts -> $(OUTDIR)/build
# Portable package     -> $(OUTDIR)/relay-c (+ .zip via build.sh)

CC ?= gcc
PYTHON ?= tools/run_python.sh
PKG_CONFIG ?= pkg-config
MINGW ?= C:/msys64/mingw64

OUTDIR ?= output
BUILDDIR := $(OUTDIR)/build
DISTDIR := $(OUTDIR)/relay-c

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

RUNTIME_DLLS := libcjson-1.dll libwinpthread-1.dll

.PHONY: all test clean package dirs version

all: dirs version $(APP) package

dirs:
	@mkdir -p $(BUILDDIR)/src $(BUILDDIR)/third_party $(BUILDDIR)/tests $(DISTDIR) $(OUTDIR)

version:
	$(PYTHON) tools/gen_version.py -o include/version_gen.h

$(APP): $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

$(BUILDDIR)/src/main.o: include/version_gen.h

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

# Portable folder: exe + DLLs + examples. Safe to copy anywhere on Windows.
package: $(APP) | dirs
	@echo "Packaging $(DISTDIR) ..."
	rm -rf "$(DISTDIR)"
	mkdir -p "$(DISTDIR)"
	cp -f "$(APP)" "$(DISTDIR)/relay-c.exe"
	@for d in $(RUNTIME_DLLS); do \
		if [ ! -f "$(MINGW)/bin/$$d" ]; then echo "ERROR: missing $(MINGW)/bin/$$d"; exit 1; fi; \
		cp -f "$(MINGW)/bin/$$d" "$(DISTDIR)/$$d"; \
		echo "  dll $$d"; \
	done
	cp -f boards.json.example "$(DISTDIR)/boards.json.example"
	cp -f tools/dist_README.txt "$(DISTDIR)/README.txt"
	tools/run_python.sh tools/dist_copy_docs.py "$(DISTDIR)"
	cp -f tools/dist_run.bat "$(DISTDIR)/run.bat"
	-cp -f $(OUTDIR)/VERSION.txt "$(DISTDIR)/VERSION.txt" 2>/dev/null || true
	@echo "OK: $(DISTDIR)"

clean:
	-rm -rf $(OUTDIR)
	-rm -f src/*.o third_party/mongoose.o relay-c.exe test_*.exe
	-rm -f libcjson-1.dll libwinpthread-1.dll
