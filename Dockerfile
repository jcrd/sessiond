FROM fedora:latest

RUN dnf install -y gcc meson git
RUN dnf install -y glib2-devel libX11-devel libXi-devel

ADD . /root/sessiond
