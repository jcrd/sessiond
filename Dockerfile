FROM debian:stable-slim

RUN apt-get update
RUN apt-get install -y meson git gcc pkgconf \
        libx11-dev libxi-dev libglib2.0-dev libudev-dev
