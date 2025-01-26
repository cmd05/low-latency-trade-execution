#pragma once

#include <iostream>
#include <chrono>

extern std::chrono::time_point<std::chrono::high_resolution_clock> g_timer_start;

// log categeries
enum class log_flags {
    none          = 0,
    ws            = 1,
    client_trader = 2,
    trade_handler = 4,
    benchmark     = 8
};

inline constexpr log_flags operator|(log_flags a, log_flags b) {
    return static_cast<log_flags>(static_cast<int>(a) | static_cast<int>(b));
}

inline constexpr log_flags operator&(log_flags a, log_flags b) {
    return static_cast<log_flags>(static_cast<int>(a) & static_cast<int>(b));
}

constexpr static log_flags ENABLED_LOG_FLAGS = (log_flags::ws 
    | log_flags::client_trader | log_flags::trade_handler | log_flags::benchmark);

// Format
// [<time>] <log type>: <msg>
#define APP_LOG(flag, message) \
    do { \
        if((ENABLED_LOG_FLAGS & flag) == log_flags::none) break; \
        std::clog << "[" << std::right << std::setw(10) \
        << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - g_timer_start).count() \
        << "ms] "; \
        if(flag == log_flags::ws) std::clog << "websocket: "; \
        else if(flag == log_flags::client_trader) std::clog << "client: "; \
        else if(flag == log_flags::trade_handler) std::clog << "trade_handler: "; \
        std::clog << message << std::endl; \
    } while(false)

// static log_flags ENABLED_PRINT_FLAGS = (log_flags::ws | log_flags::client_trader);
#define APP_PRINT(message) std::cout << message << std::endl