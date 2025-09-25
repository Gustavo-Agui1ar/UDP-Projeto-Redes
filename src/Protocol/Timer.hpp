#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <chrono>
#include <utility>

class Timer {
private:
    std::atomic<bool> running{false};
    std::thread worker{};

public:
    Timer() = default;

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    Timer(Timer&& other) noexcept {
        running = other.running.load();
        if (other.worker.joinable()) {
            worker = std::move(other.worker);
        }
        other.running = false;
    }

    Timer& operator=(Timer&& other) noexcept {
        if (this != &other) {
            stop();
            running = other.running.load();
            if (other.worker.joinable()) {
                worker = std::move(other.worker);
            }
            other.running = false;
        }
        return *this;
    }

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
