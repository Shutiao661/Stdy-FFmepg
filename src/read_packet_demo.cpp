#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main(){
    const char* filename = "data/demo.mp4";
    AVFormatContext* fmt_ctx = nullptr;

    if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) != 0) 
    {
        std::cerr << "打开文件失败\n";
        return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
    {
        std::cerr << "找不到流信息\n";
        avformat_close_input(&fmt_ctx);
        return -1;
    }
    // 1. 准备一个 AVPacket，用来接收读出来的数据包
    AVPacket* pkt = av_packet_alloc();

    int video_pkt_count = 0;
    int audio_pkt_count = 0;

    std::cout << "开始读取数据包...\n";
    // 2. 核心循环：不断从文件里读取包
    // av_read_frame 返回 0 代表成功读到一个包，返回负数（如 AVERROR_EOF）代表读完了
     while (av_read_frame(fmt_ctx, pkt) >= 0) {
        
        // pkt->stream_index 记录了当前这个包属于哪条流（0是视频，1是音频）
        if (pkt->stream_index == 0) {
            // 如果你只想知道里面装了什么，可以通过大小（size）观察
            video_pkt_count++;
        } 
        else if (pkt->stream_index == 1) {
            audio_pkt_count++;
        }

        // 3. 极其重要：每次读取完，必须释放包内部的引用计数，否则内存会撑爆！
        av_packet_unref(pkt);
    }
    std::cout << "读取完毕！\n";
    std::cout << "共读取到 " << video_pkt_count << " 个视频包\n";
    std::cout << "共读取到 " << audio_pkt_count << " 个音频包\n";

    // 4. 清理资源
    av_packet_free(&pkt);
    avformat_close_input(&fmt_ctx);
    return 0;
}