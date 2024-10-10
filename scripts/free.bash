#!/bin/sh

export NURDLIB_DEF_PATH=`pwd`/../nurdlib/cfg/default
# TODO: figure out why rpath from mvlcc doesn't stick when building outside of docker.
export LD_LIBRARY_PATH=`pwd`/../install-prefix/lib:$LD_LIBRARY_PATH

gdb --args ../nurdlib/build_cc_x86_64-linux-gnu_12_debug/m_read_meb.drasi \
	--log-no-start-wait \
        --buf=size=100Mi,valloc \
        --max-ev-size=0x100000 \
        --max-ev-interval=50s \
        --log \
        --server=trans
