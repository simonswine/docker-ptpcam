FROM ubuntu:trusty

RUN \
    apt-get update && \
    apt-get install -y lua5.2 wget unzip libcairo2 libgtk2.0-0 && \
    rm -rf /var/lib/apt/lists/*

RUN wget https://www.assembla.com/spaces/chdkptp/documents/bEQP3wxwir5ikeacwqEsg8/download/bEQP3wxwir5ikeacwqEsg8?notinline=true -O /tmp/chdkptp.zip && \
    mkdir /usr/local/chdkptp && \
    cd /usr/local/chdkptp && \
    unzip /tmp/chdkptp.zip && \
    rm /tmp/chdkptp.zip

RUN wget http://sourceforge.net/projects/iup/files/3.10.1/Linux%20Libraries/iup-3.10.1_Linux35_64_lib.tar.gz/download -O /tmp/iup.tar.gz && \
    mkdir /usr/local/iup && \
    cd /usr/local/iup && \
    tar xvfz /tmp/iup.tar.gz && \
    rm /tmp/iup.tar.gz

RUN wget http://sourceforge.net/projects/canvasdraw/files/5.9/Linux%20Libraries/cd-5.9_Linux35_64_lib.tar.gz/download -O /tmp/cd.tar.gz && \
    wget http://sourceforge.net/projects/canvasdraw/files/5.9/Linux%20Libraries/Lua52/cd-5.9-Lua52_Linux35_64_lib.tar.gz/download -O /tmp/cd-lua.tar.gz && \
    mkdir /usr/local/cd && \
    cd /usr/local/cd && \
    tar xvfz /tmp/cd.tar.gz && \
    tar xvfz /tmp/cd-lua.tar.gz && \
    rm /tmp/cd.tar.gz /tmp/cd-lua.tar.gz

ENV LD_LIBRARY_PATH=/usr/local/iup:/usr/local/cd
ENV LUA_PATH=/usr/local/chdkptp/lua/?.lua
ENV PATH=${PATH}:/usr/local/chdkptp
