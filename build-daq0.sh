# Based on info from https://fy.chalmers.se/subatom/subexp-daq/ and
# https://fy.chalmers.se/subatom/subexp-daq/minidaq_v2718_mdpp16.txt
# Author: Florian Lüke <f.lueke@mesytec.com>

set -e
set -x

# Clean some of the repos to remove build artifacts. Dangerous if your stuff is
# not commited or checked into the index!
#for d in external/{drasi,nurdlib,ucesb,mvlcc}; do
#      cd $d && git clean -d -f && cd -
#done

# Create symlinks to libs in /sources. Having another subdir in-between did
# break the build.
ln -sf external/drasi
ln -sf external/nurdlib
ln -sf external/ucesb

MAKEJOBS=32
export LC_ALL="C"
export MVLC_DIR="$PWD/install-prefix"
export MVLCC_CONFIG="external/mvlcc/bin/mvlcc-config.sh"

## the cleaning, might be needed if paths change
#rm -rf $MVLC_SRC/build
make -j$MAKEJOBS -C external/mvlcc clean
make -j$MAKEJOBS -C drasi clean-all
make -j$MAKEJOBS -C nurdlib clean

## mesytec-mvlc
MVLC_CONF_ARGS="-GNinja -DCMAKE_BUILD_TYPE=Debug -DMVLC_BUILD_TESTS=ON -DMVLC_BUILD_CONTROLLER_TESTS=OFF -DMVLC_BUILD_DEV_TOOLS=ON -DMVLC_BUILD_DOCS=OFF"
MVLC_SRC="external/mesytec-mvlc"

cmake -S $MVLC_SRC -B $MVLC_SRC/build -DCMAKE_INSTALL_PREFIX=$MVLC_DIR $MVLC_CONF_ARGS
cmake --build $MVLC_SRC/build -j$MAKEJOBS --target install
#cd $MVLC_SRC/build && ctest $MVLC_SRC/build; cd - # Doesn't work with ctest --test-dir <dir>. No clue.

## mvlcc
make -j$MAKEJOBS -C external/mvlcc
make -j$MAKEJOBS -C external/mvlcc/example

echo "MVLC_DIR=$MVLC_DIR"
export MVLCC_CFLAGS=$(external/mvlcc/bin/mvlcc-config.sh --cflags)
export MVLCC_LIBS=$(external/mvlcc/bin/mvlcc-config.sh --libs)
echo "MVLCC_CFLAGS=$MVLCC_CFLAGS"
echo "MVLCC_LIBS=$MVLCC_LIBS"

## the daq
CC=${CC:-cc}
CC_MACHINE=`$CC -dumpmachine`
CC_VERSION=`$CC -dumpversion`
BUILD_TYPE=debug
ARCH_SUFFIX=${CC_MACHINE}_${CC_VERSION}
NURD_BIN_DIR=nurdlib/build_cc_${ARCH_SUFFIX}_${BUILD_TYPE}
DAQ_BINARY=$NURD_BIN_DIR/m_read_meb.drasi

make -j$MAKEJOBS -C drasi
make -j$MAKEJOBS -C drasi showconfig showconfig_all
make -j$MAKEJOBS -C nurdlib fuser_drasi
make -j$MAKEJOBS -C nurdlib showconfig

cat nurdlib/build_cc_${ARCH_SUFFIX}_${BUILD_TYPE}/nconf/module/map/map.h.log
# Remove the symlink to caller.sh and replay it with the actualy binary.
cd nurdlib/bin && ln -sf ../../$DAQ_BINARY daq0

# For some reason ucesb builds when the tree is clean. The second time around it
# starts to run an hbook/example/ext_writer_test that never returns. git clean
# and/or make clean-all did not help.
# Update: I think this might be related to shared memory inside the container.
#make -j$MAKEJOBS -C ucesb all-clean
#make -j$MAKEJOBS -C ucesb empty
