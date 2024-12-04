#!/bin/sh
#set -x
#set -e

export NURDLIB_DEF_PATH=`pwd`/../nurdlib/cfg/default
# TODO: figure out why rpath from mvlcc doesn't stick when building outside of docker.
export LD_LIBRARY_PATH=`pwd`/../install-prefix/lib:$LD_LIBRARY_PATH

#if [ -z "$CC" ]; then
#    CC=cc
#fi
#CC_MACHINE=`$CC -dumpmachine`
#CC_VERSION=`$CC -dumpversion`
#BUILD_TYPE=debug
#ARCH_SUFFIX=${CC_MACHINE}_${CC_VERSION}
#BIN_DIR="../nurdlib/build_cc_${ARCH_SUFFIX}_${BUILD_TYPE}"
#DAQ_BINARY=$BIN_DIR/m_read_meb.drasi
DAQ_BINARY="daq3"

test -x $DAQ_BINARY || {
    echo "DAQ binary not found: $DAQ_BINARY"
    exit 1
}

# log level names: error, warning, info, debug, trace
# --log-level affects the 'daq1' logger
# --mvlc-log-level affects all mesytec-mvlc loggers

exec gdb -ex r --args $DAQ_BINARY \
	--log-no-start-wait \
        --buf=size=100Mi,valloc \
        --max-ev-size=0x100000 \
        --max-ev-interval=50s \
        --log \
        --server=trans \
        "$@"
