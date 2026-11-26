#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
}

int main() {
    const char* filename = "data/demo.mp4";
    AVFormatContext* fmt_ctx = nullptr;

    if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) != 0) {
        std::cerr << "无法打开文件\n"; return -1;
    }
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        std::cerr << "找不到流信息\n"; return -1;
    }

    // 1. 找到音频流的索引
    int audio_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
            break;
        }
    }
    if (audio_stream_index == -1) { std::cerr << "找不到音频流\n"; return -1; }

    // 2. 初始化解码器
    const AVCodec* codec = avcodec_find_decoder(fmt_ctx->streams[audio_stream_index]->codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[audio_stream_index]->codecpar);
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        std::cerr << "打开解码器失败\n"; return -1;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    std::ofstream pcm_file("data/output.pcm", std::ios::binary);
    
    int frame_count = 0;
    std::cout << "开始解码音频...\n";

    // 3. 循环解码
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == audio_stream_index) {
            if (avcodec_send_packet(codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    frame_count++;
                    int num_samples = frame->nb_samples;
                    int channels = frame->ch_layout.nb_channels;

                    // 准备一个交错格式的 int16 缓冲区
                    std::vector<int16_t> interleaved_buffer(num_samples * channels);

                    if (av_sample_fmt_is_planar(codec_ctx->sample_fmt)) {
                        // 处理平面格式 (如 fltp, s16p)
                        for (int i = 0; i < num_samples; i++) {
                            // 提取左声道数据 (如果是 fltp，转为 float 提取并放大 32767)
                            float left = ((float*)frame->data[0])[i];
                            interleaved_buffer[i * channels + 0] = (int16_t)(left * 32767.0f);
                            
                            // 提取右声道数据 (如果有的话)
                            if (channels > 1) {
                                float right = ((float*)frame->data[1])[i];
                                interleaved_buffer[i * channels + 1] = (int16_t)(right * 32767.0f);
                            }
                        }
                    } else {
                        // 处理交错格式 (如 flt, s16)
                        if (codec_ctx->sample_fmt == AV_SAMPLE_FMT_FLT) {
                            for (int i = 0; i < num_samples * channels; i++) {
                                float val = ((float*)frame->data[0])[i];
                                interleaved_buffer[i] = (int16_t)(val * 32767.0f);
                            }
                        } else if (codec_ctx->sample_fmt == AV_SAMPLE_FMT_S16) {
                            memcpy(interleaved_buffer.data(), frame->data[0], num_samples * channels * sizeof(int16_t));
                        }
                    }

                    // 写入 PCM 数据到文件
                    pcm_file.write((const char*)interleaved_buffer.data(), interleaved_buffer.size() * sizeof(int16_t));
                }
            }
        }
        av_packet_unref(pkt);
    }

    // 4. 冲洗解码器
    avcodec_send_packet(codec_ctx, nullptr);
    while (avcodec_receive_frame(codec_ctx, frame) == 0) {
        frame_count++;
        // 同样逻辑，省略重复代码
    }

    std::cout << "音频解码完成！共解码出 " << frame_count << " 个音频帧。\n";
    std::cout << "已保存为 16bit 交错 PCM 文件: data/output.pcm\n";

    // 5. 释放资源
    pcm_file.close();
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);
    return 0;
}