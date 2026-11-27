#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>       // 修复 av_opt_* 标红
#include <libavutil/channel_layout.h>
#include <SDL2/SDL.h>
}

int main() {
    const char* filename = "data/demo.mp4";
    AVFormatContext* fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) != 0) return -1;
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) return -1;

    int video_idx = -1, audio_idx = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) video_idx = i;
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) audio_idx = i;
    }

    const AVCodec* vcodec = avcodec_find_decoder(fmt_ctx->streams[video_idx]->codecpar->codec_id);
    AVCodecContext* vctx = avcodec_alloc_context3(vcodec);
    avcodec_parameters_to_context(vctx, fmt_ctx->streams[video_idx]->codecpar);
    avcodec_open2(vctx, vcodec, nullptr);

    const AVCodec* acodec = avcodec_find_decoder(fmt_ctx->streams[audio_idx]->codecpar->codec_id);
    AVCodecContext* actx = avcodec_alloc_context3(acodec);
    avcodec_parameters_to_context(actx, fmt_ctx->streams[audio_idx]->codecpar);
    avcodec_open2(actx, acodec, nullptr);

    // 1. 初始化 SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window* window = SDL_CreateWindow("My AV Player", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, vctx->width, vctx->height, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, vctx->width, vctx->height);

    // 2. 配置 SDL 音频
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = actx->sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = actx->ch_layout.nb_channels;
    want.samples = 1024;
    want.callback = nullptr; 
    SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    SDL_PauseAudioDevice(audio_dev, 0); 

    // 3. 初始化重采样器 (更通用、兼容性极强的老写法，避免 av_opt 标红)
    SwrContext* swr_ctx = swr_alloc();
    // 创建输入和输出的声道布局
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, want.channels); 
    
    av_opt_set_chlayout(swr_ctx, "out_chlayout", &out_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", want.freq, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    
    av_opt_set_chlayout(swr_ctx, "in_chlayout", &actx->ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", actx->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", actx->sample_fmt, 0);

    if (swr_init(swr_ctx) < 0) {
        std::cerr << "重采样器初始化失败！\n";
        return -1;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    std::vector<uint8_t> pcm_buffer;
    std::vector<AVPacket*> video_pkt_queue;

    // 取消 goto，使用标志位控制循环
    bool is_running = true;

    while (is_running) {
        // 优先喂饱音频
        double queued_seconds = (double)SDL_GetQueuedAudioSize(audio_dev) / (want.freq * want.channels * 2);
        
        if (queued_seconds < 0.1) {
            if (av_read_frame(fmt_ctx, pkt) >= 0) {
                if (pkt->stream_index == audio_idx) {
                    if (avcodec_send_packet(actx, pkt) == 0) {
                        while (avcodec_receive_frame(actx, frame) == 0) {
                            int out_samples = av_rescale_rnd(swr_get_delay(swr_ctx, actx->sample_rate) + frame->nb_samples,
                                                             want.freq, actx->sample_rate, AV_ROUND_UP);
                            pcm_buffer.resize(out_samples * want.channels * sizeof(int16_t));
                            
                            uint8_t* out_data = pcm_buffer.data();
                            int actual_samples = swr_convert(swr_ctx, &out_data, out_samples, (const uint8_t**)frame->data, frame->nb_samples);
                            
                            if (actual_samples > 0) {
                                int bytes_to_queue = actual_samples * want.channels * sizeof(int16_t);
                                SDL_QueueAudio(audio_dev, pcm_buffer.data(), bytes_to_queue);
                            }
                        }
                    }
                } else if (pkt->stream_index == video_idx) {
                    AVPacket* vpkt = av_packet_alloc();
                    av_packet_move_ref(vpkt, pkt);
                    video_pkt_queue.push_back(vpkt);
                }
                av_packet_unref(pkt);
            } else {
                // 文件读完了
                is_running = false; 
            }
        } 
        else if (!video_pkt_queue.empty()) {
            AVPacket* vpkt = video_pkt_queue.front();
            video_pkt_queue.erase(video_pkt_queue.begin());

            if (avcodec_send_packet(vctx, vpkt) == 0) {
                while (avcodec_receive_frame(vctx, frame) == 0) {
                    double queued = (double)SDL_GetQueuedAudioSize(audio_dev) / (want.freq * want.channels * 2);
                    if (queued > 0.05) {
                        SDL_Delay(static_cast<Uint32>((queued - 0.05) * 1000));
                    }

                    SDL_UpdateYUVTexture(texture, nullptr,
                        frame->data[0], frame->linesize[0],
                        frame->data[1], frame->linesize[1],
                        frame->data[2], frame->linesize[2]);
                    
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                    SDL_RenderPresent(renderer);

                    // 处理退出事件
                    SDL_Event e;
                    while (SDL_PollEvent(&e)) {
                        if (e.type == SDL_QUIT) {
                            is_running = false; // 优雅退出
                            break;
                        }
                    }
                }
            }
            av_packet_free(&vpkt);
        } 
        else {
            SDL_Delay(10); 
        }
    }

    // 释放资源
    SDL_CloseAudioDevice(audio_dev);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    swr_free(&swr_ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    for (auto vpkt : video_pkt_queue) {
        av_packet_free(&vpkt);
    }
    avformat_close_input(&fmt_ctx);
    return 0;
}