# syntax=docker/dockerfile:1.5
# vim:ft=dockerfile

FROM debian:stable as build

ENV DEBIAN_FRONTEND="noninteractive"
ENV TZ="Etc/UTC"

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates build-essential git cmake ssh

COPY . /sources/
WORKDIR /sources/

ENTRYPOINT ["bash"]
