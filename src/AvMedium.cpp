#include "AvMedium.h"

#include <SDL2/SDL.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <ranges>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

AvMedium::AvMedium(const std::string& _url) : m_url{_url}
{
    std::invoke(av_log_set_level, AV_LOG_DEBUG);
    std::invoke(&AvMedium::set_dict, this, m_url);
    if (!std::invoke(&AvMedium::av_init, this))
    {
        return;
    }
}

AvMedium::~AvMedium() noexcept
{
    avformat_free_context(m_format_ctx);
    spdlog::info("Media playback has been turned off");
}

#define OPENCV
// #define SDL2

#ifdef OPENCV
auto AvMedium::read() noexcept -> void
{
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);
    cv::namedWindow("Video Playback", cv::WINDOW_NORMAL);

    // 先确保 m_frame->format 是有效的 AVPixelFormat
    AVPixelFormat src_fmt = static_cast<AVPixelFormat>(m_codec_ctx->pix_fmt);
    int           src_w   = m_codec_ctx->width;
    int           src_h   = m_codec_ctx->height;

    if (src_fmt == AV_PIX_FMT_NONE)
    {
        std::cerr << "Invalid pixel format!" << std::endl;
        return;
    }

    // 初始化 sws_context（从源格式转换到 BGR24）
    SwsContext* sws_ctx = sws_getContext(
        src_w, src_h, src_fmt,
        src_w, src_h, AV_PIX_FMT_BGR24,
        SWS_LANCZOS, nullptr, nullptr, nullptr);
    // SWS_BICUBIC 双三次插值
    // SWS_LANCZOS 高清图片
    // SWS_FAST_BILINEAR 低延迟低质量图片

    if (!sws_ctx)
    {
        std::cerr << "Failed to create SwsContext!" << std::endl;
        return;
    }

    while (av_read_frame(m_format_ctx, m_packet) >= 0)
    {
        if (m_packet->stream_index == m_video_index)
        {
            if (avcodec_send_packet(m_codec_ctx, m_packet) == 0)
            {
                while (avcodec_receive_frame(m_codec_ctx, m_frame) == 0)
                {
                    // 创建 OpenCV 图像
                    cv::Mat img_bgr(src_h, src_w, CV_8UC3);

                    // 准备输出缓冲区（仅需一平面）
                    uint8_t* dst_data[1]     = {img_bgr.data};
                    int      dst_linesize[1] = {static_cast<int>(img_bgr.step[0])};

                    // 转换图像格式 YUV → BGR
                    sws_scale(sws_ctx, m_frame->data, m_frame->linesize,
                              0, src_h, dst_data, dst_linesize);

                    // 显示图像
                    cv::imshow("Video Playback", img_bgr);
                    if (cv::waitKey(1) == 27)  // 按 ESC 退出
                    {
                        goto finish;
                    }
                }
            }
        }
        av_packet_unref(m_packet);
    }

finish:
    sws_freeContext(sws_ctx);
    cv::destroyAllWindows();
}
#endif

#ifdef SDL2
auto AvMedium::read() noexcept -> void
{
    // 初始化SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL初始化失败: " << SDL_GetError() << std::endl;
        return;
    }

    // 获取视频参数
    AVPixelFormat src_fmt = static_cast<AVPixelFormat>(m_codec_ctx->pix_fmt);
    int           src_w   = m_codec_ctx->width;
    int           src_h   = m_codec_ctx->height;

    if (src_fmt == AV_PIX_FMT_NONE)
    {
        std::cerr << "无效的像素格式!" << std::endl;
        SDL_Quit();
        return;
    }

    // 创建SDL窗口和渲染器
    SDL_Window* window = SDL_CreateWindow(
        "Video Playback",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        src_w, src_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        std::cerr << "无法创建SDL窗口: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!renderer)
    {
        std::cerr << "无法创建SDL渲染器: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // 创建SDL纹理
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_BGR24,
        SDL_TEXTUREACCESS_STREAMING,
        src_w, src_h);

    if (!texture)
    {
        std::cerr << "无法创建SDL纹理: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // 初始化图像转换上下文
    SwsContext* sws_ctx = sws_getContext(
        src_w, src_h, src_fmt,
        src_w, src_h, AV_PIX_FMT_BGR24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!sws_ctx)
    {
        std::cerr << "无法创建SwsContext!" << std::endl;
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // 创建帧缓冲区
    AVFrame* bgr_frame = av_frame_alloc();
    bgr_frame->format  = AV_PIX_FMT_BGR24;
    bgr_frame->width   = src_w;
    bgr_frame->height  = src_h;
    // 分配内存缓冲区
    av_frame_get_buffer(bgr_frame, 0);

    bool      running = true;
    SDL_Event event;

    while (running && av_read_frame(m_format_ctx, m_packet) >= 0)
    {
        // 处理SDL事件
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
            {
                running = false;
            }
        }

        if (m_packet->stream_index == m_video_index)
        {
            if (avcodec_send_packet(m_codec_ctx, m_packet) == 0)
            {
                while (avcodec_receive_frame(m_codec_ctx, m_frame) == 0)
                {
                    // 转换图像格式
                    sws_scale(sws_ctx,
                              m_frame->data, m_frame->linesize,
                              0, src_h,
                              bgr_frame->data, bgr_frame->linesize);

                    // 更新SDL纹理
                    SDL_UpdateTexture(texture, nullptr, bgr_frame->data[0], bgr_frame->linesize[0]);

                    // 渲染
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                    SDL_RenderPresent(renderer);
                }
            }
        }
        av_packet_unref(m_packet);
    }

    // 清理资源
    av_frame_free(&bgr_frame);
    sws_freeContext(sws_ctx);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
#endif

auto AvMedium::set_dict(std::string _url) noexcept -> AVDictionary*
{
    AVDictionary* __options{nullptr};
    if (_url.find("rtsp://") == 0) [[likely]]
    {
        // 强制使用 TCP
        av_dict_set(&__options, "rtsp_transport", "tcp", 0);

        // 5秒超时
        av_dict_set(&__options, "stimeout", "5000000", 0);

        // 设置分析持续时间为较小的值（例如 1000000 微秒，即 1 秒）
        av_dict_set(&__options, "analyzeduration", "1", 0);

        // 设置探测大小为较小的值（例如 5000000 字节）
        av_dict_set(&__options, "probesize", "300000", 0);

        // 设置最大延迟（例如，100ms）
        av_dict_set(&__options, "max_delay", "100", 0);

        // 禁用内部缓冲，减少延迟
        av_dict_set(&__options, "fflags", "ignidx+nobuffer+nofillin+discardcorrupt", 0);

        // 选择 GPU 设备
        // av_dict_set(&__options, "hwaccel", "cuda", 0);
        // av_dict_set(&__options, "hwaccel_device", "0", 0);

        // 延迟最低
        av_dict_set(&__options, "flags", "low_delay", 0);

        // 非阻塞模式
        av_dict_set_int(&__options, "avioflags", AVIO_FLAG_NONBLOCK, 0);

        // 控制为流索引（timestamp index）分配的最大内存1MB
        av_dict_set(&__options, "indexmem", "1048576", 0);

        // 启用 RTP MP4A-LATM Payload
        av_dict_set(&__options, "latm", "1", 0);

        // 降低帧率
        // av_dict_set(&__options, "r", "30", 0);

        // 帧丢弃（防止堵塞）
        av_dict_set(&__options, "framedrop", "1", 0);

        // 增加缓冲
        av_dict_set(&__options, "buffer_size", "32768", 0);

        // 断线重连
        av_dict_set(&__options, "reconnect", "1", 0);

        // 流结束后重连
        av_dict_set(&__options, "reconnect_at_eof", "1", 0);

        av_dict_set(&__options, "reconnect_streamed", "1", 0);
        av_dict_set(&__options, "reconnect_delay_max", "5", 0);

        av_dict_set(&__options, "err_detect", "none", 0);  // 禁用错误检测
        av_dict_set(&__options, "crccheck", "0", 0);       // 禁用 CRC 校验
        av_dict_set(&__options, "bitstream", "0", 0);      // 禁用比特流检测
        av_dict_set(&__options, "buffer", "0", 0);         // 禁用比特流长度检测
        av_dict_set(&__options, "explode", "0", 0);        // 禁用错误中止
        av_dict_set(&__options, "careful", "0", 0);        // 禁用严格错误检测
        av_dict_set(&__options, "compliant", "0", 0);      // 禁用规范性检查
        av_dict_set(&__options, "aggressive", "0", 0);     // 禁用过度检查
        // av_dict_set(&__options, "use_wallclock_as_timestamps", "0", 0);  // 禁用墙钟时间作为时间戳
        av_dict_set(&__options, "skip_initial_bytes", "0", 0);  // 禁用跳过初始字节
    }
    else if (_url.find("rtmp://") == 0)
    {
        av_dict_set(&__options, "rtmp_tcp_nodelay", "1", 0);
        av_dict_set(&__options, "rtmp_buffer", "32768", 0);
        av_dict_set(&__options, "timeout", "5000000", 0);
        av_dict_set(&__options, "fflags", "nobuffer+flush_packets", 0);
        av_dict_set(&__options, "flags", "low_delay", 0);
        av_dict_set(&__options, "reconnect", "1", 0);
        av_dict_set(&__options, "reconnect_at_eof", "1", 0);
        av_dict_set(&__options, "reconnect_delay_max", "5", 0);
    }
    else if (_url.find("http://") == 0 || _url.find("https://") == 0)
    {
        // 5秒超时
        av_dict_set(&__options, "stimeout", "5000000", 0);
        av_dict_set(&__options, "fflags", "nobuffer+flush_packets", 0);
        av_dict_set(&__options, "flags", "low_delay", 0);
        av_dict_set(&__options, "analyzeduration", "100000", 0);
        av_dict_set(&__options, "probesize", "50000", 0);
        av_dict_set(&__options, "reconnect", "1", 0);
    }
    // av_dict_set(&__options, "sync", "video", 0);
    return __options;
}

auto AvMedium::av_init() noexcept -> bool
{
    AVDictionary* format_options{this->set_dict(m_url)};
    if (!format_options)
    {
        spdlog::debug("The input address isn't live:{}", m_url);
    }
    if (avformat_open_input(&m_format_ctx, m_url.c_str(), nullptr, &format_options) < 0)
    {
        spdlog::error("The input address is invalid:{}", m_url);
        return false;
    }
    av_dict_free(&format_options);

    // av_log(nullptr, AV_LOG_DEBUG, "The input address is:%s\n", m_format_ctx->url);
    spdlog::debug("The input address's format:{}", m_format_ctx->iformat->name);

    // if (m_format_ctx->duration != AV_NOPTS_VALUE)
    // {
    //     std::cout << "Duration: " << (m_format_ctx->duration / 1e6) << " sec" << std::endl;
    // }
    // else
    // {
    //     std::cout << "Duration not available" << std::endl;
    // }
    if (avformat_find_stream_info(m_format_ctx, nullptr) < 0)
    {
        spdlog::error("Could not find stream:{}", m_format_ctx->nb_streams);
        return false;
    }

    m_video_index = av_find_best_stream(m_format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_video_index < 0)
    {
        spdlog::error("Could not find video stream:{}", m_video_index);
        return false;
    }
    // 从视频流中提取编解码参数（如编码格式、分辨率、帧率等）
    AVCodecParameters* codec_parameters{m_format_ctx->streams[m_video_index]->codecpar};
    // std::cout << codec_parameters->width << '\n';
    // std::cout << codec_parameters->height << '\n';
    // std::cout << static_cast<double>(codec_parameters->framerate.num) / codec_parameters->framerate.den << '\n';
#if 1
    m_codec_ctx = avcodec_alloc_context3(nullptr);
    if (!m_codec_ctx)
    {
        spdlog::error("Could not allocate codec context");
        return false;
    }
    // m_codec_ctx->codec_id      = AV_CODEC_ID_H264;      // 指定使用的编码器为 H.264
    // m_codec_ctx->codec_type    = AVMEDIA_TYPE_VIDEO;    // 表示当前上下文是用于“视频”而非音频或字幕
    m_codec_ctx->thread_count = 4;                // 设置编码时使用的线程数为 8
    m_codec_ctx->thread_type  = FF_THREAD_FRAME;  // 表示当前上下文是用于“视频”而非音频或字幕
    // m_codec_ctx->bit_rate      = 8000000;               // 设置目标码率为 8 Mbps
    // const AVRational framerate = {30, 1};               // 定义帧率为 30fps
    // m_codec_ctx->time_base     = av_inv_q(framerate);   // 每一帧之间的时间间隔是 1/30秒
    // m_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;  // 将一些编码器头信息（如 SPS、PPS）写到 extradata 中
    // m_codec_ctx->flags2 |= AV_CODEC_FLAG_PASS2;         // 双通道编码的第二通

    if (avcodec_parameters_to_context(m_codec_ctx, codec_parameters) < 0)
    {
        spdlog::error("Could not copy codec parameters to context");
        return false;
    }

    const AVCodec* codec{avcodec_find_decoder(codec_parameters->codec_id)};
    if (!codec)
    {
        spdlog::error("Could supported codec");
        return false;
    }
    AVDictionary* opt = nullptr;
    av_dict_set(&opt, "crf", "23", 0);
    av_dict_set(&opt, "rc_mode", "CBR", 0);
    av_dict_set(&opt, "preset", "medium", 0);
    av_dict_set(&opt, "tune", "zerolatency", 0);
    av_dict_set(&opt, "x264-params", "keyint=30:min-keyint=30;profile=high", 0);
    if (avcodec_open2(m_codec_ctx, codec, &opt) < 0)
    {
        spdlog::error("Could not allocate codec context");
        return false;
    }
    av_dict_free(&opt);
#endif
    return true;
}
