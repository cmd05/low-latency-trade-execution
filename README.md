# Low Latency Trade Execution System using Deribit API

A command line trading application, optimized for low latency trading requests using websockets. It implements Deribit API, however the application uses a dependency-inversion pattern so that using another API is as simple as creating a C++ class derived from the common trading interface class.

- The application uses a multithreaded implementation for network requests and user interaction. At the time of testing, websocket requests were on average below **100us**.

Deribit API Reference: https://docs.deribit.com/

## Demo

https://github.com/user-attachments/assets/bb979f66-d2bb-4e06-b3b6-34fc246b32c6

## Usage

1. Rename the file `api_key.json.sample` in the project-root directory to `api_key.json`. Add the client-id and client-secret from you Deribit API Key (found in the API section in the settings)
2. Make sure you have built the application as per the [build section](#build)
3. Run the built executable `client_trader`.
4. The commands provided in the application can be seen using `help`. The first step is to connect to the Deribit API using `deribit_connect`
5. Authorize to the Deribit API using `deribit_auth` (this will use the API credentials provided in `api_key.json`. Note: This step is recommended, however commands using `public/` API methods such as `deribit_order_book` can be used without authorization)
6. Use the available commands to perform trading operations

![Pasted image 20250123224943.png](./_assets/Pasted%20image%2020250123224943.png)

## Features

### Order Management Functions
- Placing buy and sell orders
- Edit and Cancel orders
- Get orderbook (`/public/get_order_book`)
- View current positions

### Realtime Market Data

Stream realtime market data by subcribing to one or more channels. Channel names can be referred to using the API documentation.

### Market Coverage
The application supports Spot, Futures, Options and Perpetual trading as instruments. All supported symbols have been implemented.

## Build

The repository requires Boost and OpenSSL package which must be discoverable by CMake on your system using CMake's `find_package`.

The following commands should be used from the project root to build the application.
```sh
cmake -B build
cmake --build build -j$(nproc) # speed-up compilation
```

An executable called `client_trader` will be created in the project root.

## Performance Analysis
A detailed analysis of profiling and benchmarking can be found in the [Performance Report](./Performance%20Report.md) document.

## Code Review
The explanation of the code can be found in the [Code Review](./Code%20Review.md) document.

## Libraries used

* [websocketspp](https://github.com/zaphoyd/websocketpp)
* [nlohmann json](https://github.com/nlohmann/json)
