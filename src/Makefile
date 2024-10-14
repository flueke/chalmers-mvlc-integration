# CPPFLAGS and LIB handling taken from r3bfuser/Makefile
NURDLIB_PATH:=../nurdlib
include $(NURDLIB_PATH)/gmake/build_dir.mk
NURDLIB_A:=$(NURDLIB_PATH)/$(BUILD_DIR)/libnurdlib.a
DRASI_CONFIG=../drasi/bin/drasi-config.sh
MVLC_DIR=../install-prefix

CPPFLAGS:=$(CPPFLAGS) \
	-I. \
	-I$(NURDLIB_PATH) \
	-I$(NURDLIB_PATH)/include \
	-I$(NURDLIB_PATH)/$(BUILD_DIR)

CPPFLAGS:=$(CPPFLAGS) -DFUSER_DRASI=1 $(shell $(DRASI_CONFIG) --f-user-header --mbscompat --cflags)
CPPFLAGS:=$(CPPFLAGS) -Wall -Wextra -ggdb -Wshadow -fdiagnostics-color=auto -fPIC -std=c++17
CPPFLAGS:=$(CPPFLAGS) -I$(MVLC_DIR)/include -isystem $(MVLC_DIR)/include/mesytec-mvlc
CPPFLAGS:=$(CPPFLAGS) -Wno-dangling-reference # silence erroneous spdlog warnings emitted by some g++ versions.
CPPFLAGS:=$(CPPFLAGS) -Wno-literal-suffix # suppress a C++ PRIz related warning
CPPFLAGS:=$(CPPFLAGS) -Wno-register # suppress warnings about 'register' keyword not being a c++ thing
CPPFLAGS:=$(CPPFLAGS) -fpermissive # LOGL() void * -> LogFile * conversion

LIBS:=$(shell $(DRASI_CONFIG) --f-user-daq --mbscompat --libs) -L${NURDLIB_PATH}/${BUILD_DIR} -lnurdlib
LIBS:=$(LIBS) -L${MVLC_DIR}/lib/ -lmesytec-mvlc -lstdc++ -Wl,-rpath=${MVLC_DIR}/lib

ifeq (debug,$(BUILD_MODE))
 CPPFLAGS+=-ggdb
endif
ifeq (gprof,$(BUILD_MODE))
 CPPFLAGS+=-ggdb -pg
endif
ifeq (pic,$(BUILD_MODE))
 CPPFLAGS+=-fPIC
endif
ifeq (release,$(BUILD_MODE))
 CPPFLAGS+=-O3
endif

.PHONY: all clean info

all: daq1

clean:
	-rm -f daq1.o daq1

daq1.o: daq1.cc

daq1: daq1.o
	$(CXX) -o $@ $^ $(LIBS)
