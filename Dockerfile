# syntax=docker/dockerfile:1.5
# vim:ft=dockerfile

FROM debian:bullseye as build

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
RUN build-daqs.sh
