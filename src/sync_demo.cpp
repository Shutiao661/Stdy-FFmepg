#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

// 模拟：音频时钟（单位：毫秒）
double get_audio_clock() {
    // 真实工程中：声卡已经播放了多少字节 / (采样率 * 声道数 * 位深/8) * 1000
    // 这里假设音频在第 1000ms 的位置
    return 1000.0; 
}

// 模拟：获取下一帧视频的 PTS（单位：毫秒）
// 我们在视频流里按帧取，假设帧率是 60fps，每帧间隔约 16.6ms
double get_next_video_pts(double& current_video_pts) {
    current_video_pts += 16.66; 
    return current_video_pts;
}

int main() {
    // 模拟当前视频帧的 PTS，假设刚开始视频落后音频一点
    double video_pts = 950.0; 
    
    // 同步阈值：如果音画差距在 50ms 以内，就当没看见，正常播
    const double SYNC_THRESHOLD = 50.0; 
    // 丢弃阈值：如果视频落后音频超过 150ms，必须丢帧
    const double DROP_THRESHOLD = 150.0; 

    std::cout << "开始模拟音视频同步...\n";

    // 模拟播放 10 帧视频
    for (int i = 0; i < 10; i++) {
        // 1. 获取当前音频进度和即将播放的视频帧进度
        double audio_clock = get_audio_clock();
        double current_video_pts = get_next_video_pts(video_pts);
        
        // 2. 计算差值
        double diff = current_video_pts - audio_clock;
        
        std::cout << "[" << i << "] 音频时钟: " << audio_clock 
                  << "ms | 视频PTS: " << current_video_pts 
                  << "ms | 差值: " << diff << "ms -> ";

        // 3. 同步逻辑判断
        if (diff > SYNC_THRESHOLD) {
            // 视频超前，睡眠等待
            std::cout << "视频超前，等待 " << diff << "ms\n";
            // 真实代码: std::this_thread::sleep_for(std::chrono::milliseconds((int)diff));
            
        } else if (diff < -DROP_THRESHOLD) {
            // 视频落后太多，丢弃此帧
            std::cout << "视频严重落后，直接丢弃此帧！\n";
            continue; // 跳过当前帧的渲染
            
        } else {
            // 差值在 -150ms 到 +50ms 之间，属于正常范围
            std::cout << "差距正常，直接渲染画面\n";
            // 真实代码: render_frame(frame);
        }

        // 模拟视频渲染耗时为 5ms
        // std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    return 0;
}