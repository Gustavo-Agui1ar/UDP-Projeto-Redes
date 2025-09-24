#pragma once

#include <thread>
#include <chrono>
#include <atomic>
#include <functional>
#include <iostream>

class Timer {

private:
    
    std::atomic<bool> running{false};
    std::thread worker;

public:

    Timer() = default;

    ~Timer() {
        stop();
    }

    void start(int intervalMs, std::function<void()> callback) {
        stop(); 
        running = true;

        worker = std::thread([=]() mutable {
            while (running) {
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
                if (running) callback();
            }
        });
    }

    void stop() {
        running = false;
        if (worker.joinable()) {
            worker.join();
        }
    }
};
