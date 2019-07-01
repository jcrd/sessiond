FROM debian:stable-slim

# enable testing repo
RUN echo 'APT::Default-Release "stable";' > \
        /etc/apt/apt.conf.d/99defaultrelease

RUN cat /etc/apt/sources.list | sed 's/stable/testing/g' > \
        /etc/apt/sources.list.d/testing.list

RUN apt-get update
# meson >= 0.47.0 required for 'features' option
RUN apt-get -t testing install -y meson
# glib >= 2.52 required for GUuid
RUN apt-get -t testing install -y libglib2.0-dev
RUN apt-get install -y git gcc pkgconf libx11-dev libxi-dev
