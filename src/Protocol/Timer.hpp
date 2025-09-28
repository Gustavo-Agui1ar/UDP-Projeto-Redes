#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <unordered_set>
#include <cstdint>

class Timer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Callback = std::function<void()>;
    using Id = uint64_t;

private:
    struct Task {
        TimePoint expiry;
        Id id;
        Callback cb;
        bool operator>(Task const& o) const { return expiry > o.expiry; }
    };

    std::priority_queue<Task, std::vector<Task>, std::greater<Task>> pq;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> running{false};
    std::atomic<Id> nextId{1};
    std::unordered_set<Id> cancelled;

public:
    Timer() {
        running = true;
        worker = std::thread([this]() { loop(); });
    }

    ~Timer() {
        stop();
    }

    Id addTimeout(int intervalMs, Callback cb) {
        auto id = nextId.fetch_add(1, std::memory_order_relaxed);
        Task t{Clock::now() + std::chrono::milliseconds(intervalMs), id, std::move(cb)};
        {
            std::lock_guard<std::mutex> lock(mtx);
            pq.push(std::move(t));
        }
        cv.notify_one();
        return id;
    }

    void cancel(Id id) {
        std::lock_guard<std::mutex> lock(mtx);
        cancelled.insert(id);
        cv.notify_one();
    }

    void stop() {
        running = false;
        cv.notify_all();
        if (worker.joinable()) worker.join();
    }

private:
    void loop() {
        std::unique_lock<std::mutex> lock(mtx);
        while (running) {
            if (pq.empty()) {
                cv.wait(lock, [&]() { return !running || !pq.empty(); });
                continue;
            }

            auto now = Clock::now();
            auto &t = pq.top();
            if (t.expiry <= now) {
                Task task = std::move(const_cast<Task&>(t));
                Id id = task.id;
                pq.pop();

                if (cancelled.erase(id)) {
                    continue;
                }

                lock.unlock();
                try {
                    task.cb();
                } catch (...) {
                }
                lock.lock();
            } else {
                cv.wait_until(lock, t.expiry);
            }
        }
    }
};
