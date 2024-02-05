FROM kathara/p4

RUN mkdir -p /ns3

WORKDIR /ns3

# Install Python2.7 needed by waf
RUN apt update && apt install -y \
    mercurial \
    build-essential \
    libsqlite3-dev \
    zlib1g-dev \
    libncurses5-dev \
    libgdbm-dev \
    libbz2-dev \
    libreadline-dev \
    libssl-dev \
    libdb-dev && \
wget http://www.python.org/ftp/python/2.7.18/Python-2.7.18.tgz && \
tar -xzf Python-2.7.18.tgz && \
cd Python-2.7.18 && \
./configure --prefix=/usr --enable-shared && \
make && make install && \
update-alternatives --install /usr/bin/python python /usr/bin/python2.7 10 && \
update-alternatives --set python /usr/bin/python2.7

RUN git clone https://gitlab.com/nsnam/ns-3-allinone.git && \
    cd ns-3-allinone && \
    ./download.py -n ns-3.29 && \
    cd ns-3.29 && \
    CXXFLAGS="-W -Wall -g" ./waf configure --enable-examples && \
    ./waf build

COPY ns-3.29/src/p4-switch /ns3/ns-3-allinone/ns-3.29/src/p4-switch
COPY ns-3.29/src/wscript /ns3/ns-3-allinone/ns-3.29/src/wscript

RUN cd ns-3-allinone/ns-3.29 && \
    CXXFLAGS="-W -Wall -g" ./waf configure --enable-examples && \
    ./waf build

RUN rm -Rf Python-2.7.18* && \
    apt remove -y mercurial && \
    apt clean && \
    rm -rf /tmp/* /var/lib/apt/lists/* /var/tmp/*

RUN chmod +x /ns3/ns-3-allinone/ns-3.29/src/p4-switch/helper/run_bmv2_CLI

ENV PATH="$PATH:/ns3/ns-3-allinone/ns-3.29/src/p4-switch/helper"

VOLUME /ns3/assets