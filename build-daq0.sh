set -e
#set -x

MAKEJOBS=32
MVLC_CONF_ARGS="-DMVLC_BUILD_TESTS=OFF -DMVLC_BUILD_CONTROLLER_TESTS=OFF -DMVLC_BUILD_DEV_TOOLS=OFF -DMVLC_BUILD_DOCS=OFF"
MVLC_SRC="external/mesytec-mvlc"
export MVLC_DIR="install-prefix"
export MVLCC_CONFIG="external/mvlcc/bin/mvlcc-config.sh"

cmake -DCMAKE_BUILD_TYPE=Release -S $MVLC_SRC -B $MVLC_SRC/build \
      -DCMAKE_INSTALL_PREFIX=$MVLC_DIR $MVLC_CONF_ARGS
cmake --build $MVLC_SRC/build -j$MAKEJOBS --target install

# Create symlinks to libs in /sources. Having another subdir in-between did
# break the build.
ln -sf external/drasi
ln -sf external/nurdlib
ln -sf external/ucesb

make -j$MAKEJOBS -C external/mvlcc
make -j$MAKEJOBS -C drasi
make -j$MAKEJOBS -C drasi showconfig showconfig_all

# This is too much at times :)
sed -i -e '/pedantic-errors/d' nurdlib/Makefile

export MVLCC_CFLAGS=$(external/mvlcc/bin/mvlcc-config.sh --cflags)
export MVLCC_LIBS=$(external/mvlcc/bin/mvlcc-config.sh --libs)

echo "MVLCC_CFLAGS=$MVLCC_CFLAGS"
echo "MVLCC_LIBS=$MVLCC_LIBS"

make -j$MAKEJOBS -C nurdlib clean
make -j$MAKEJOBS -C nurdlib fuser_drasi
make -j$MAKEJOBS -C nurdlib showconfig
make -j$MAKEJOBS -C ucesb all-clean

# Skipping ucesb for now. For some reason it builds when the tree is clean. The
# second time around it starts to run an hbook/example/ext_writer_test that
# never returns. git clean and/or make clean-all did not help.
#cd ucesb && git clean -d -f && cd ..
#make -j$MAKEJOBS -C ucesb clean
#make -j$MAKEJOBS -C ucesb empty

cat nurdlib/build_cc_x86_64-linux-gnu_12_debug/nconf/module/map/map.h.log
