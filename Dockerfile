# syntax=docker/dockerfile:1.5
# vim:ft=dockerfile

# To build:
#   Place the extracted CAENVMELib-v4.0.2  in external/
#   git submodule update --init --recursive
#   docker build -t chalmers.se-mvlc:latest --progress plain .

FROM debian:stable as build

ENV DEBIAN_FRONTEND="noninteractive"
ENV TZ="Etc/UTC"

# libusb-1.0 is required by CAENVMELib
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates build-essential git cmake ssh curl bison \
    flex libncurses-dev libusb-1.0-0-dev

# Temp stuff during development
RUN apt-get install -y --no-install-recommends vim file bash-completion less gdb
RUN echo "set nocompatible\nset bg=dark\nsyntax enable\n" >> ~/.vimrc

COPY . /sources/
WORKDIR /sources

ARG MAKEJOBS=8
ARG CAENVERSION="v4.0.2"
ENV CAENLIB_DIR="/sources/CAENVMELib-${CAENVERSION}"
ENV CAENLIB_SO="$CAENLIB_DIR/lib/x64/libCAENVME.so.$CAENVERSION"
ENV CPPFLAGS="-I$CAENLIB_DIR/include"
ENV LIBS=${CAENLIB_SO}

# Create symlinks to libs in /sources. Having another subdir in-between did
# break the build.
RUN ln -s external/CAENVMELib-v4.0.2
RUN ln -s external/drasi
RUN ln -s external/nurdlib
RUN ln -s external/ucesb

# Remove -pedantic-errors from CFLAGS in the nurdlib Makefile. CAENVMELib
# produces warnings about C99 variadic macros, which are turned into errors by
# this flag. Note: also removes -ansi as the whole line is deleted.
RUN sed -i -e '/pedantic-errors/d' nurdlib/Makefile

RUN make -j${MAKEJOBS} -C drasi
RUN make -j${MAKEJOBS} -C drasi showconfig showconfig_all
RUN make -j${MAKEJOBS} -C nurdlib fuser_drasi
RUN make -j${MAKEJOBS} -C nurdlib showconfig
RUN make -j${MAKEJOBS} -C ucesb empty

WORKDIR /sources/scripts
RUN  curl -O https://fy.chalmers.se/subatom/subexp-daq/minidaq_v2718_mdpp16/main.cfg
RUN  curl -O https://fy.chalmers.se/subatom/subexp-daq/minidaq_v2718_mdpp16/free.bash

WORKDIR /sources
ENTRYPOINT ["bash"]
