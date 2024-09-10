# syntax=docker/dockerfile:1.5
# vim:ft=dockerfile

FROM debian:stable as build

ARG MAKEJOBS=8
ENV DEBIAN_FRONTEND="noninteractive"
ENV TZ="Etc/UTC"

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates build-essential git cmake ssh curl bison \
    flex libncurses-dev

COPY . /sources/
WORKDIR /sources

RUN make -j${MAKEJOBS} -C external/drasi
RUN make -j${MAKEJOBS} -C external/nurdlib
RUN make -j${MAKEJOBS} -C external/ucesb empty


ENTRYPOINT ["bash"]
