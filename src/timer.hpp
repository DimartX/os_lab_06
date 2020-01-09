#pragma once

#include <chrono>

struct Timer {
    using milli = std::chrono::milliseconds;
    
    Timer() {
        start_ = std::chrono::system_clock::now();
        end_ = start_;
        timer_on_ = false;
    }

    void start() {
        timer_on_ = true;
        start_ = std::chrono::system_clock::now();    
    }

    void stop() {
        end_ = std::chrono::system_clock::now();
        timer_on_ = false;
    }

    int time() {
        if (timer_on_)
            end_ = std::chrono::system_clock::now();
        return std::chrono::duration_cast<milli>(end_ - start_).count();
    }

    bool status() {
        return timer_on_;
    }
private:
    std::chrono::time_point<std::chrono::system_clock> start_;
    std::chrono::time_point<std::chrono::system_clock> end_;
    bool timer_on_;
};

