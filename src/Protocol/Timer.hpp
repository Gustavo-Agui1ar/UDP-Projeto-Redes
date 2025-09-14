#include <thread>
#include <chrono>
#include <atomic>
#include <functional>
#include <iostream>

class Timer {

private:
    
    atomic<bool> running{false};
    thread worker;

public:

    Timer() = default;

    ~Timer() {
        stop();
    }

    void start(int intervalMs, function<void()> callback) {
        stop(); 
        running = true;

        worker = thread([=]() mutable {
            while (running) {
                this_thread::sleep_for(chrono::milliseconds(intervalMs));
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
