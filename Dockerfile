# syntax=docker/dockerfile:1.5
# vim:ft=dockerfile

FROM debian:stable as build

ENV DEBIAN_FRONTEND="noninteractive"
ENV TZ="Etc/UTC"

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates build-essential git cmake ssh curl bison \
    flex libncurses-dev

# temp stuff during development
RUN apt-get install -y --no-install-recommends vim file bash-completion
RUN echo "set bg=dark\nsyntax enable\n" >> ~/.vimrc

COPY . /sources/
WORKDIR /sources

ARG MAKEJOBS=8
ARG CAENVERSION="v4.0.2"
ARG CAENLIB_DIR="/sources/CAENVMELib-${CAENVERSION}"
ENV CPPFLAGS="-I${CAENLIB_DIR}/include -DHAS_CAENVME=1 -DHWMAP_RW_FUNC=1"
ENV LIBS="${CAENLIB_DIR}/lib/x64/libCAENVME.so.${CAENVERSION}"

# Create symlinks to libs in /sources. Having another subdir in-between breaks
# some stuff (what stuff?).
RUN ln -s external/CAENVMELib-v4.0.2
RUN ln -s external/drasi
RUN ln -s external/nurdlib
RUN ln -s external/ucesb

RUN make -j${MAKEJOBS} -C drasi
RUN make -j${MAKEJOBS} -C drasi showconfig showconfig_all
RUN make -j${MAKEJOBS} -C nurdlib fuser_drasi
RUN make -j${MAKEJOBS} -C nurdlib showconfig
RUN make -j${MAKEJOBS} -C ucesb empty

ENTRYPOINT ["bash"]
