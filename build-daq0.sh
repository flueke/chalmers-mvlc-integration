# Based on info from https://fy.chalmers.se/subatom/subexp-daq/ and
# https://fy.chalmers.se/subatom/subexp-daq/minidaq_v2718_mdpp16.txt
# Author: Florian Lüke <f.lueke@mesytec.com>

set -e
set -x

# Clean some of the repos to remove build artifacts.
for d in external/{drasi,nurdlib,ucesb,mvlcc}; do
      cd $d && git clean -d -f && cd -
done

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
#make -j$MAKEJOBS -C external/mvlcc clean
#make -j$MAKEJOBS -C drasi clean-all
#make -j$MAKEJOBS -C nurdlib clean

## mesytec-mvlc
MVLC_CONF_ARGS="-DCMAKE_BUILD_TYPE=Debug -DMVLC_BUILD_TESTS=OFF -DMVLC_BUILD_CONTROLLER_TESTS=OFF -DMVLC_BUILD_DEV_TOOLS=OFF -DMVLC_BUILD_DOCS=OFF"
MVLC_SRC="external/mesytec-mvlc"

cmake -S $MVLC_SRC -B $MVLC_SRC/build -DCMAKE_INSTALL_PREFIX=$MVLC_DIR $MVLC_CONF_ARGS
cmake --build $MVLC_SRC/build -j$MAKEJOBS --target install

## mvlcc
echo "MVLC_DIR=$MVLC_DIR"
export MVLCC_CFLAGS=$(external/mvlcc/bin/mvlcc-config.sh --cflags)
export MVLCC_LIBS=$(external/mvlcc/bin/mvlcc-config.sh --libs)
echo "MVLCC_CFLAGS=$MVLCC_CFLAGS"
echo "MVLCC_LIBS=$MVLCC_LIBS"

make -j$MAKEJOBS -C external/mvlcc

## the daq
make -j$MAKEJOBS -C drasi
make -j$MAKEJOBS -C drasi showconfig showconfig_all
make -j$MAKEJOBS -C nurdlib fuser_drasi
make -j$MAKEJOBS -C nurdlib showconfig

# For some reason ucesb builds when the tree is clean. The second time around it
# starts to run an hbook/example/ext_writer_test that never returns. git clean
# and/or make clean-all did not help.
# Update: I think this might be related to shared memory inside the container.
#make -j$MAKEJOBS -C ucesb all-clean
#make -j$MAKEJOBS -C ucesb empty

cat nurdlib/build_cc_x86_64-linux-gnu_12_debug/nconf/module/map/map.h.log
