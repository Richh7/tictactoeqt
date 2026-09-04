FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV QT_X11_NO_MITSHM=1

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    qt6-base-dev \
    qt6-qpa-plugins \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt ./
COPY client ./client
COPY server ./server

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && cp build/tictactoe_client build/tictactoe_server /usr/local/bin/

EXPOSE 8080

CMD ["tictactoe_server"]
