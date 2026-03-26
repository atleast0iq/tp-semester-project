set shell := ["zsh", "-cu"]

[doc("Show available recipes")]
default:
    @just --list

[doc("Configure Debug build for both client and server")]
configure:
    cmake --preset clang-debug

[doc("Configure Debug build for both client and server")]
configure-debug:
    cmake --preset clang-debug

[doc("Configure Debug build for the client only")]
configure-client:
    cmake --preset clang-client-debug

[doc("Configure Debug build for the server only")]
configure-server:
    cmake --preset clang-server-debug

[doc("Configure Release build for both client and server")]
configure-release:
    cmake --preset clang-release

[doc("Build Debug preset for both client and server")]
build: configure-debug
    cmake --build --preset build-debug

[doc("Build Debug preset for both client and server")]
build-debug: configure-debug
    cmake --build --preset build-debug

[doc("Build Debug preset for the client only")]
build-client: configure-client
    cmake --build --preset build-client

[doc("Build Debug preset for the server only")]
build-server: configure-server
    cmake --build --preset build-server

[doc("Build Release preset for both client and server")]
build-release: configure-release
    cmake --build --preset build-release

[doc("Remove all generated build directories and top-level compile_commands.json")]
clean:
    rm -rf build/clang-debug
    rm -rf build/clang-client-debug
    rm -rf build/clang-server-debug
    rm -rf build/clang-release
    rm -f compile_commands.json

[doc("Rebuild Debug preset for both client and server from scratch")]
rebuild: clean build-debug

[doc("Rebuild Debug preset for the client from scratch")]
rebuild-client:
    rm -rf build/clang-client-debug
    cmake --preset clang-client-debug
    cmake --build --preset build-client

[doc("Rebuild Debug preset for the server from scratch")]
rebuild-server:
    rm -rf build/clang-server-debug
    cmake --preset clang-server-debug
    cmake --build --preset build-server

[doc("Run the Debug server build")]
run-server port="4242" database="battleship.db" address="0.0.0.0": build-server
    ./build/clang-server-debug/battleship_server --address {{address}} --port {{port}} --database {{database}}

[doc("Run the Debug client build")]
run-client host="127.0.0.1" port="4242": build-client
    ./build/clang-client-debug/battleship_client --host {{host}} --port {{port}}

[doc("Build the Docker image for the server")]
docker-build:
    docker compose build

[doc("Start the server with docker compose")]
docker-up:
    docker compose up --build

[doc("Stop docker compose services")]
docker-down:
    docker compose down
