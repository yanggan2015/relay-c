# windows_desktop — relay_desktop.exe (WebView2 shell, Windows-only)
# Build: mingw32-make -C windows_desktop
# Or from repo root: mingw32-make desktop
#
# Shared webview: ../../ssh-bridge-c/third_party (sibling repo)

CXX ?= g++
MINGW ?= C:/msys64/mingw64
ROOT := ..
OUTDIR ?= $(ROOT)/output
BUILDDIR := $(OUTDIR)/build/windows_desktop
APP := $(OUTDIR)/build/relay_desktop.exe
APP_COPY := $(OUTDIR)/relay_desktop.exe
RES := $(BUILDDIR)/desktop_res.o
WEBVIEW_ROOT := $(ROOT)/../ssh-bridge-c/third_party
WEBVIEW2_DLL := $(WEBVIEW_ROOT)/webview2/x64/WebView2Loader.dll

# Prefer MSYS bash so mkdir -p / cp / test work under recursive make from cmd.
ifneq ($(wildcard C:/msys64/usr/bin/bash.exe),)
  SHELL := C:/msys64/usr/bin/bash.exe
endif

CXXFLAGS ?= -O2 -Wall -std=c++17 -D_WIN32_WINNT=0x0601 \
	-finput-charset=UTF-8 -fexec-charset=UTF-8 \
	-I. -I$(WEBVIEW_ROOT)/webview \
	-I$(WEBVIEW_ROOT)/webview2/include

LIBS := -mwindows -municode -static-libgcc -static-libstdc++ \
	-ladvapi32 -lole32 -loleaut32 -lshell32 -lshlwapi \
	-luser32 -lversion -lws2_32

.PHONY: all clean dirs

all: dirs $(APP)

dirs:
	@mkdir -p $(BUILDDIR) $(dir $(APP)) $(OUTDIR)

$(RES): desktop.rc relay_desktop.ico desktop_res.h | dirs
	windres -I. -O coff -o $(RES) desktop.rc

$(APP): main.cpp $(RES) $(WEBVIEW_ROOT)/webview/webview.h | dirs
	@test -f $(WEBVIEW_ROOT)/webview2/include/WebView2.h || \
		(echo "ERROR: missing WebView2.h — see ssh-bridge-c/third_party/webview2"; exit 1)
	$(CXX) $(CXXFLAGS) -o $@ main.cpp $(RES) $(LIBS)
	cp -f $@ $(APP_COPY)
	@if [ -f "$(WEBVIEW2_DLL)" ]; then \
		cp -f "$(WEBVIEW2_DLL)" "$(OUTDIR)/build/WebView2Loader.dll"; \
		cp -f "$(WEBVIEW2_DLL)" "$(OUTDIR)/WebView2Loader.dll"; \
	fi

clean:
	-rm -rf $(BUILDDIR)
	-rm -f $(APP) $(APP_COPY)
