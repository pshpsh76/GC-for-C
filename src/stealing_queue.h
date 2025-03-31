#include <deque>
#include <mutex>

template <typename T>
class WorkStealingQueue {
public:
    void push(T a) {
        std::lock_guard<std::mutex> lock(mtx_);
        dq_.push_back(a);
    }

    bool pop(T& a) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (dq_.empty()) {
            return false;
        }
        a = std::move(dq_.back());
        dq_.pop_back();
        return true;
    }

    bool steal(T& a) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (dq_.empty()) {
            return false;
        }
        a = std::move(dq_.front());
        dq_.pop_front();
        return true;
    }

private:
    std::deque<T> dq_;
    std::mutex mtx_;
};
