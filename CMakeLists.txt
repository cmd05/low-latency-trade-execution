cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0076 NEW)

include(FetchContent)

project(client_trader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# boost library
find_package(Boost REQUIRED COMPONENTS system thread)

# openssl library
find_package(OpenSSL REQUIRED)

# nlohmann-json library
# https://json.nlohmann.me/integration/cmake/#fetchcontent
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(json)

# websocketpp library
# https://github.com/zaphoyd/websocketpp/issues/990
FetchContent_Declare(websocketpp
    GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
    GIT_TAG master
)

FetchContent_MakeAvailable(websocketpp)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -pg -g1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
# set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -pg")

add_executable(client_trader)

target_sources(client_trader
    PRIVATE
    src/client_main.cpp
    src/websocket/websocket.cpp
    src/client/client_trader.cpp
)

target_include_directories(client_trader 
    PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${websocketpp_SOURCE_DIR}
)

target_include_directories(client_trader
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(client_trader 
    PRIVATE 
    OpenSSL::SSL
    OpenSSL::Crypto
    Boost::system
    Boost::thread
    nlohmann_json::nlohmann_json
)

set_target_properties(client_trader PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/")