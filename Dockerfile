FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    g++ \
    git \
    default-libmysqlclient-dev \
    libhiredis-dev \
    libboost-dev \
    libcrypt-dev \
    && rm -rf /var/lib/apt/lists/*

RUN cd /tmp && git clone https://github.com/chenshuo/muduo.git && \
    cd muduo && ./build.sh && ./build.sh install && \
    cp -r /tmp/release-install-cpp11/include/muduo /usr/local/include/ && \
    cp -d /tmp/release-install-cpp11/lib/libmuduo_*.so* /usr/local/lib/ && \
    ldconfig

WORKDIR /app
COPY . /app

RUN cmake -S . -B build && cmake --build build -j2

CMD ["./bin/chat_server", "--config", "config/server.docker.conf"]
