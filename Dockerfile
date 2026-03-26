FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    clang \
    cmake \
    ninja-build \
    qt6-base-dev \
    libqt6sql6-sqlite \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build/docker-server -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DBATTLESHIP_BUILD_SERVER=ON \
    -DBATTLESHIP_BUILD_CLIENT=OFF \
    && cmake --build build/docker-server --target battleship_server

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    qt6-base-dev \
    libqt6sql6-sqlite \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/battleship

COPY --from=build /app/build/docker-server/battleship_server ./battleship_server
COPY --from=build /app/build/docker-server/migrations ./migrations

EXPOSE 4242
VOLUME ["/data"]

ENTRYPOINT ["./battleship_server"]
CMD ["--address", "0.0.0.0", "--port", "4242", "--database", "/data/battleship.db"]
