# syntax=docker/dockerfile:1.5
# vim:ft=dockerfile

# To build:
#   git submodule update --init --recursive
#   docker build -t chalmers.se-mvlc:latest --progress plain .

FROM debian:stable as build

ENV DEBIAN_FRONTEND="noninteractive"
ENV TZ="Etc/UTC"

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates build-essential git cmake ssh curl bison \
    flex libncurses-dev cmake

# Temp stuff during development (bsdextrautils is for the hexdump utility)t
RUN apt-get install -y --no-install-recommends vim file bash-completion less gdb bsdextrautils
RUN echo "set nocompatible\nset bg=dark\nsyntax enable\n" >> ~/.vimrc

COPY . /sources/
WORKDIR /sources

ARG MAKEJOBS=8
#ARG CAENVERSION="v4.0.2"
#ENV CAENLIB_DIR="/sources/CAENVMELib-${CAENVERSION}"
#ENV CAENLIB_SO="$CAENLIB_DIR/lib/x64/libCAENVME.so.$CAENVERSION"
#ENV CPPFLAGS="-I$CAENLIB_DIR/include"
#ENV LIBS=${CAENLIB_SO}
ARG MVLC_DIR="/sources/external/mesytec-mvlc"
ENV MVLC_DIR=$MVLC_DIR
ENV MVLCC_CONFIG=external/mvlcc/bin/mvlcc-config.sh

RUN cmake -DCMAKE_BUILD_TYPE=Release -S $MVLC_DIR -B $MVLC_DIR/build \
        -DCMAKE_INSTALL_PREFIX=$MVLC_DIR/install \
        && cmake --build $MVLC_DIR/build -j$MAKEJOBS --target install

# Create symlinks to libs in /sources. Having another subdir in-between did
# break the build.
RUN ln -s external/drasi
RUN ln -s external/nurdlib
RUN ln -s external/ucesb

RUN make -j$MAKEJOBS -C external/mvlcc


RUN make -j$MAKEJOBS -C drasi
RUN make -j$MAKEJOBS -C drasi showconfig showconfig_all

# No clue how to make nconf execute things, so stuff is run here manually and
# written to an env file.
RUN echo export MVLCC_CFLAGS=\"$(./external/mvlcc/bin/mvlcc-config.sh --cflags)\" > /tmp/mvlcc_flags.env
RUN echo export MVLCC_LIBS=\"$(./external/mvlcc/bin/mvlcc-config.sh --libs)\" >> /tmp/mvlcc_flags.env
RUN cat /tmp/mvlcc_flags.env
RUN . /tmp/mvlcc_flags.env && make -j$MAKEJOBS -C nurdlib fuser_drasi
RUN make -j$MAKEJOBS -C nurdlib showconfig
RUN make -j$MAKEJOBS -C ucesb empty
RUN cat nurdlib/build_cc_x86_64-linux-gnu_12_debug/nconf/module/map/map.h.log

#WORKDIR /sources/scripts
#RUN  curl -O https://fy.chalmers.se/subatom/subexp-daq/minidaq_v2718_mdpp16/main.cfg
#RUN  curl -O https://fy.chalmers.se/subatom/subexp-daq/minidaq_v2718_mdpp16/free.bash

WORKDIR /sources
ENTRYPOINT ["bash"]
