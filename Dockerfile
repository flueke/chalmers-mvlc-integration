# syntax=docker/dockerfile:1.5
# vim:ft=dockerfile

FROM debian:stable as build

ENV DEBIAN_FRONTEND="noninteractive"
ENV TZ="Etc/UTC"

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates build-essential git cmake ssh curl bison \
    flex libncurses-dev

# temp stuff during development
RUN apt-get install -y --no-install-recommends vim

COPY . /sources/
WORKDIR /sources

ARG MAKEJOBS=8
ARG CAENVERSION="v4.0.2"
ARG CAENLIB_DIR="/sources/external/CAENVMELib-${CAENVERSION}"
ENV CPPFLAGS="-I${CAENLIB_DIR}/include"
ENV LIBS="${CAENLIB_DIR}/lib/x64/libCAENVME.so.${CAENVERSION}"

RUN ln -s external/CAENVMELib-v4.0.2
RUN ln -s external/drasi
RUN ln -s external/nurdlib
RUN ln -s external/ucesb

RUN make -j${MAKEJOBS} -C drasi
RUN make -j${MAKEJOBS} -C drasi showconfig
RUN make -j${MAKEJOBS} -C nurdlib fuser_drasi
RUN make -j${MAKEJOBS} -C nurdlib showconfig
RUN make -j${MAKEJOBS} -C ucesb empty
RUN cd scripts && ./free.bash

ENTRYPOINT ["bash"]
