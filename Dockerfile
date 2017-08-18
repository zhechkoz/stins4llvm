FROM ubuntu:16.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    bc bison \
    cmake \
    curl \
    flex \
    git \
    jq \
    libboost-all-dev libcap-dev libncurses5-dev \
    locales \
    nano \
    python-minimal python3 python3.5-dev \
    subversion \
    sudo \
    unzip \
    vim \
    wget \
    zlib1g-dev && \
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

# Add a non-root user
RUN useradd --create-home --shell /bin/bash sip && \
    adduser sip sudo && \
    echo 'sip:sip' | chpasswd

WORKDIR /home/sip

# Build KLEE
RUN git clone --depth 1 https://github.com/tum-i22/klee-install.git && \
    mkdir -p build && \
    sh ./klee-install/ubuntu.sh /home/sip/build/

RUN git clone -b ktest-result --depth 1 https://github.com/tum-i22/macke-opt-llvm.git /home/sip/build/macke-opt-llvm && \
    make -C /home/sip/build/macke-opt-llvm -j 2 \
    LLVM_SRC_PATH=/home/sip/build/llvm \
    KLEE_BUILDDIR=/home/sip/build/klee/Release+Asserts \
    KLEE_INCLUDES=/home/sip/build/klee/include

# Install jsoncpp
RUN git clone --depth 1 --branch 1.8.1  https://github.com/open-source-parsers/jsoncpp.git && \
    mkdir -p /home/sip/jsoncpp/build && \
    cmake -H/home/sip/jsoncpp -B/home/sip/jsoncpp/build && \
    make -C /home/sip/jsoncpp/build -j 2 install

# Switch to user sip
RUN chown -R sip:sip /home/sip/
USER sip
WORKDIR /home/sip

# Set up project repo
RUN git clone https://github.com/zhechkoz/stins4llvm.git

WORKDIR /home/sip/stins4llvm
RUN make
