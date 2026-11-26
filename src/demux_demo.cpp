#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main() {
    const char* filename = "data/demo.mp4";
    AVFormatContext* fmt_ctx = nullptr;

    // 1. 打开文件，读取文件头
    if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) != 0) {
        std::cerr << "无法打开文件: " << filename << "\n";
        return -1;
    }
    std::cout << "成功打开文件: " << filename << "\n";

    // 2. 探测文件内部的流信息
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "找不到流信息\n";
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    // 打印总览信息
    std::cout << "文件时长: " << fmt_ctx->duration / AV_TIME_BASE << " 秒\n";
    std::cout << "包含的流数量: " << fmt_ctx->nb_streams << " 条\n\n";

    // 3. 遍历每一条流
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        AVStream* stream = fmt_ctx->streams[i];
        AVCodecParameters* codecpar = stream->codecpar;

        std::cout << "=== 第 " << i << " 条流 ===\n";

        // 判断是视频流还是音频流
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            std::cout << "  [类型] 视频流\n";
            std::cout << "  [编码格式] " << avcodec_get_name(codecpar->codec_id) << "\n";
            std::cout << "  [分辨率] " << codecpar->width << " x " << codecpar->height << "\n";
            AVRational fr = stream->avg_frame_rate;
            std::cout << "  [帧率] " << fr.num / fr.den << " fps\n";
        } 
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            std::cout << "  [类型] 音频流\n";
            std::cout << "  [编码格式] " << avcodec_get_name(codecpar->codec_id) << "\n";
            std::cout << "  [采样率] " << codecpar->sample_rate << " Hz\n";
            std::cout << "  [声道数] " << codecpar->ch_layout.nb_channels << "\n";
        } 
        else {
            std::cout << "  [类型] 其他流\n";
        }
        std::cout << "\n";
    }

    // 4. 释放资源
    avformat_close_input(&fmt_ctx);
    return 0;
}