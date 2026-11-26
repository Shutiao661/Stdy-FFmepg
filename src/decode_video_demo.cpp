#include <iostream>
#include <fstream>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
int main (){
    const char* filename = "data/demo.mp4";
    AVFormatContext* fmt_ctx = nullptr;

    // 1. 打开文件
    if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) != 0) return -1;
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) return -1;

    // 2. 找到视频流的索引 (index)
    int video_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) { std::cerr << "找不到视频流\n"; return -1;}
        
        // 3. 初始化解码器 
        // 第一步：找医生 (H.264 解码器)
        const AVCodec* codec = avcodec_find_decoder(fmt_ctx->streams[video_stream_index]->codecpar->codec_id);
        if(!codec){std::cerr << "找不到解码器\n"; return -1;}
        // 第二步：建病历本 (解码器上下文)
        AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
        // 第三步：把流参数告诉解码器
        avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);
        // 第四步：正式开诊
        if (avcodec_open2(codec_ctx, codec, nullptr) < 0) { std::cerr << "打开解码器失败\n"; return -1; }
        // 4. 准备容器
        AVPacket* pkt = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        // 5. 准备一个文件，用来保存解出来的 YUV 裸数据（验证用）
        std::ofstream yuv_file("data/output.yuv", std::ios::binary);
        int frame_count = 0;
        std::cout << "开始解码...\n";
        // 6. 循环读取数据包
        while (av_read_frame(fmt_ctx, pkt) >= 0)
        {
            if (pkt->stream_index == video_stream_index)
            {
                // 发送压缩包给解码器
                if (avcodec_send_packet(codec_ctx, pkt) == 0)
                {
                    // 循环接收解码后的帧（可能一个包解出 0 帧、1 帧或多帧）
                    while (avcodec_receive_frame(codec_ctx, frame) == 0)
                    {
                        frame_count++;
                        // 将 YUV 数据写入文件 (这里假设是 YUV420P 格式)
                        // Y 分量
                        for (int i = 0; i < frame->height; i++)
                        {
                            yuv_file.write((const char*)frame->data[0] + i * frame->linesize[0], frame->width);
                        }
                        // U 分量 (高度和宽度减半)
                        for (int i = 0; i < frame->height / 2; i++)
                        {
                            yuv_file.write((const char*)frame->data[1] + i * frame->linesize[1], frame->width / 2);
                        }
                        // V 分量 (高度和宽度减半)
                        for (int i = 0; i < frame->height / 2; i++)
                        {
                            yuv_file.write((const char*)frame->data[2] + i * frame->linesize[2], frame->width / 2);
                        }
                    }
                }
            }
            av_packet_unref(pkt);
        }
        // 7. 刷新解码器 (Flush)
        // 因为 B 帧的存在，解码器内部可能还缓存了几帧没吐出来，必须发一个空包进去冲洗干净
        avcodec_send_packet(codec_ctx, nullptr);
        while (avcodec_receive_frame(codec_ctx, frame) == 0) 
        {
            frame_count++;
            // 写入文件逻辑同上，省略重复代码
        }
        std::cout << "解码完成！共解码出 " << frame_count << " 帧 YUV 画面。\n";
        std::cout << "YUV 文件已保存至 data/output.yuv\n";
        // 8. 清理
        yuv_file.close();
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);

        return 0;       
}
