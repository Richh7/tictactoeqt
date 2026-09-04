# TicTacToe Qt

TicTacToe client/server game written with Qt 6.

## Local Build

Build the project locally with CMake:

```bash
cmake -S . -B build
cmake --build build
```

### Starting the Server

Start the server with:

```bash
./build/tictactoe_server
```

### Starting the Clients

Start two clients in separate terminals:

```bash
./build/tictactoe_client
```

## Docker Build

Build the Docker image with:

```bash
docker build -t tictactoeqt .
```

## Running the Server in Docker

The server listens on port `8080`.

```bash
docker run --rm --name tictactoe-server -p 8080:8080 tictactoeqt
```

Alternatively, you can explicitly specify the server executable:

```bash
docker run --rm --name tictactoe-server -p 8080:8080 tictactoeqt tictactoe_server
```

## Running the Client in Docker on Linux/X11

First, allow local Docker containers to access the X11 display:

```bash
xhost +local:docker
```

Then start two clients in separate terminals:

```bash
docker run --rm \
  -e DISPLAY=$DISPLAY \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  --network host \
  tictactoeqt tictactoe_client
```

The client connects to the server using:

```text
Host: 127.0.0.1
Port: 8080
```

Once you are finished, revoke the X11 permission:

```bash
xhost -local:docker
```