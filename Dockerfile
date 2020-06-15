FROM debian:stable-slim

RUN apt-get update
RUN apt-get install -y meson
RUN apt-get install -y git
RUN apt-get install -y gcc
RUN apt-get install -y pkgconf
RUN apt-get install -y libx11-dev
RUN apt-get install -y libxi-dev
RUN apt-get install -y libglib2.0-dev
RUN apt-get install -y libudev-dev
