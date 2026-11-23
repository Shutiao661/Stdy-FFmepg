#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

// 这是一个线程安全的队列，专门用于多线程之间传递数据
class SafeQueue {
public:
    // 生产者调用：把数据塞进队列
    void push(const std::vector<char>& data) {
        std::unique_lock<std::mutex> lock(mtx_); // 加锁，防止多人同时改队列
        q_.push(data);
        cond_.notify_one(); // 通知一个正在等待的消费者：“有货了，快来拿！”
    }

    // 消费者调用：从队列取数据
    bool pop(std::vector<char>& out_data) {
        std::unique_lock<std::mutex> lock(mtx_);
        // 如果队列为空，且还没有结束，就阻塞在这里等（释放锁并休眠，不占 CPU）
        cond_.wait(lock, [this]() { return !q_.empty() || is_shutdown_; });
        
        if (q_.empty()) {
            return false; // 说明是 shutdown 了，且没货了
        }
        
        out_data = std::move(q_.front()); // 用 move 提高性能，避免内存拷贝
        q_.pop();
        return true;
    }

    // 关闭队列（通知消费者线程该下班了）
    void shutdown() {
        std::unique_lock<std::mutex> lock(mtx_);
        is_shutdown_ = true;
        cond_.notify_all(); // 叫醒所有等待的消费者
    }

private:
    std::queue<std::vector<char>> q_;
    std::mutex mtx_;
    std::condition_variable cond_;
    bool is_shutdown_ = false;
};