FROM kathara/p4

RUN mkdir -p /ns3

WORKDIR /ns3

ARG DEBIAN_FRONTEND="noninteractive"
RUN apt update && apt install -y gdb cmake build-essential

COPY ns-3.40 /ns3/ns-3.40

RUN cd ns-3.40 && \
    CXXFLAGS="-W -Wall -g" ./ns3 configure --enable-examples && \
    ./ns3 build

RUN apt clean && \
    rm -rf /tmp/* /var/lib/apt/lists/* /var/tmp/*

RUN chmod +x /ns3/ns-3.40/src/p4-switch/helper/run_bmv2_CLI

ENV PATH="$PATH:/ns3/ns-3.40/src/p4-switch/helper"

VOLUME /ns3/assets