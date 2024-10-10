# syntax=docker/dockerfile:1.5
# vim:ft=dockerfile

# Based on info from https://fy.chalmers.se/subatom/subexp-daq/ and
# https://fy.chalmers.se/subatom/subexp-daq/minidaq_v2718_mdpp16.txt

FROM debian:stable as build

ARG UID=1000
ENV DEBIAN_FRONTEND="noninteractive"
ENV TZ="Etc/UTC"

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates build-essential git cmake ssh curl bison \
    flex libncurses-dev cmake

# Temp stuff during development (bsdextrautils is for the hexdump utility)t
RUN apt-get install -y --no-install-recommends vim file bash-completion less gdb bsdextrautils tmux iputils-ping
RUN echo "set nocompatible\nset bg=dark\nsyntax enable\n" >> ~/.vimrc

RUN adduser -u $UID daq
USER daq

COPY . /sources/
WORKDIR /sources

ARG MAKEJOBS=32
ARG MVLC_DIR="/sources/external/mesytec-mvlc"
ARG MVLC_CONF_ARGS="-DMVLC_BUILD_TESTS=OFF -DMVLC_BUILD_CONTROLLER_TESTS=OFF -DMVLC_BUILD_DEV_TOOLS=OFF -DMVLC_BUILD_DOCS=OFF"
ENV MVLC_DIR=$MVLC_DIR
ENV MVLCC_CONFIG=external/mvlcc/bin/mvlcc-config.sh

RUN cmake -DCMAKE_BUILD_TYPE=Release -S $MVLC_DIR -B $MVLC_DIR/build \
        -DCMAKE_INSTALL_PREFIX=$MVLC_DIR/install $MVLC_CONF_ARGS \
        && cmake --build $MVLC_DIR/build -j$MAKEJOBS --target install

# Create symlinks to libs in /sources. Having another subdir in-between did
# break the build.
RUN ln -s external/drasi
RUN ln -s external/nurdlib
RUN ln -s external/ucesb

RUN make -j$MAKEJOBS -C external/mvlcc

RUN make -j$MAKEJOBS -C drasi
RUN make -j$MAKEJOBS -C drasi showconfig showconfig_all

# This is too much at times :)
RUN sed -i -e '/pedantic-errors/d' nurdlib/Makefile

# No clue how to make nconf execute things, so stuff is run here manually and
# written to an env file.
RUN echo export MVLCC_CFLAGS=\"$(./external/mvlcc/bin/mvlcc-config.sh --cflags)\" > /tmp/mvlcc_flags.env
RUN echo export MVLCC_LIBS=\"$(./external/mvlcc/bin/mvlcc-config.sh --libs)\" >> /tmp/mvlcc_flags.env
RUN cat /tmp/mvlcc_flags.env
RUN . /tmp/mvlcc_flags.env && make -j$MAKEJOBS -C nurdlib fuser_drasi
RUN make -j$MAKEJOBS -C nurdlib showconfig
RUN make -j$MAKEJOBS -C ucesb empty
RUN cat nurdlib/build_cc_x86_64-linux-gnu_12_debug/nconf/module/map/map.h.log

WORKDIR /sources/scripts
#ENTRYPOINT ["bash", "./free.bash"]
