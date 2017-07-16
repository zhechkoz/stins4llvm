FROM ubuntu:16.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    jq \
    locales \
    nano \
    sudo \
    vim \
    wget && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Specify locales
RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

# Install llvm 3.9 (based on http://apt.llvm.org/)
RUN echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-3.9 main" >> /etc/apt/sources.list && \
    echo "deb-src http://apt.llvm.org/xenial/ llvm-toolchain-xenial-3.9 main"  >> /etc/apt/sources.list && \
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 15CF4D18AF4F7421

RUN apt-get update && apt-get install -y \
    clang-3.9 clang-3.9-doc \
    libclang-common-3.9-dev libclang-3.9-dev libclang1-3.9 libclang1-3.9-dbg \
    libllvm-3.9-ocaml-dev libllvm3.9 libllvm3.9-dbg \
    lldb-3.9 llvm-3.9 llvm-3.9-dev llvm-3.9-doc llvm-3.9-examples llvm-3.9-runtime \
    clang-format-3.9 \
    python-clang-3.9 \
    libfuzzer-3.9-dev && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Install angr
RUN apt-get update && apt-get install -y \
    libffi-dev \
    python \
    python-pip && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
RUN pip install angr

# Add a non-root user
RUN useradd --create-home --shell /bin/bash sip && \
    adduser sip sudo && \
    echo 'sip:sip' | chpasswd

WORKDIR /home/sip

# Install KLEE dependencies
RUN apt-get update && apt-get install -y \
    bc bison \
    curl flex \
    libboost-all-dev libcap-dev libncurses5-dev \
    python-minimal \
    subversion \
    unzip \
    zlib1g-dev && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

# Build KLEE
RUN git clone https://github.com/tum-i22/klee-install.git && \
    mkdir build && \
    sh ./klee-install/ubuntu.sh /home/sip/build/

# Build KLEE opt passes
RUN apt-get update && apt-get install -y \
    python3 \
    python3.5-dev \
    python3-pip && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

WORKDIR /home/sip/build
RUN git clone -b ktest-result --depth 1 https://github.com/tum-i22/macke-opt-llvm.git
WORKDIR /home/sip/build/macke-opt-llvm
RUN make -j 2 \
    LLVM_SRC_PATH=/home/sip/build/llvm \
    KLEE_BUILDDIR=/home/sip/build/klee/Release+Asserts \
    KLEE_INCLUDES=/home/sip/build/klee/include/

WORKDIR /home/sip

# Install radare2
RUN git clone https://github.com/radare/radare2 && \
    sh /home/sip/radare2/sys/install.sh && \
    pip3 install r2pipe

# Install project dependencies
RUN git clone https://github.com/open-source-parsers/jsoncpp.git
WORKDIR /home/sip/jsoncpp/build
RUN cmake .. && \
    make -j 2 install

# Switch to user sip
RUN chown -R sip:sip /home/sip/
USER sip
WORKDIR /home/sip

# Set up project repo
RUN git clone https://github.com/johanneszi/SIP.git

WORKDIR /home/sip/SIP/phase3/stins4llvm
RUN make
