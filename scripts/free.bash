#!/bin/sh

export NURDLIB_DEF_PATH=`pwd`/../nurdlib/cfg/default

../nurdlib/bin/m_read_meb.drasi \
	--log-no-start-wait \
        --buf=size=100Mi,valloc \
        --max-ev-size=0x100000 \
        --max-ev-interval=50s \
        --log \
        --server=trans
