FROM debian:stretch-slim

# enable stretch backports
RUN echo 'deb http://deb.debian.org/debian stretch-backports main' > \
        /etc/apt/sources.list.d/stretch-backports.list

RUN apt-get update
# meson >= 0.47.0 required for 'features' option
RUN apt-get -t stretch-backports install -y meson
RUN apt-get install -y git gcc pkgconf libglib2.0-dev libx11-dev libxi-dev
