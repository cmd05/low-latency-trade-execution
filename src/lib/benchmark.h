#pragma once

#include <chrono>

#include <lib/utilities.h>

class benchmark {
public:
    benchmark(std::string lab): label{lab} {}
    
    void reset(std::string lab = "") {
        started = false;
        label = lab;
    }

    void start() {
        APP_LOG(log_flags::benchmark, "Started benchmark: " << label);
        started = true;
        start_time = std::chrono::high_resolution_clock::now();
    }

    void end() {
        if(!started) {
            // APP_LOG(log_flags::benchmark, "Benchmark not started: " << label);
            return;
        }

        end_time = std::chrono::high_resolution_clock::now();

        auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
        APP_LOG(log_flags::benchmark, "Benchmark: " << label << ", took " << elapsed_time << " us");

        started = false;
    }
private:
    std::string label;

    bool started = false;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time;
};