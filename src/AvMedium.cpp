#include "AvMedium.h"

#include <SDL2/SDL.h>
#include <spdlog/spdlog.h>

#include <boost/url.hpp>
#include <functional>
#include <opencv2/opencv.hpp>
#include <print>
#include <ranges>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
}

AvMedium::AvMedium(const std::string& _url) : m_url{_url}
{
    std::invoke(av_log_set_level, AV_LOG_DEBUG);
    std::invoke(&AvMedium::checkUrl, this);
    std::invoke(&AvMedium::avideoHandle, this);
}

AvMedium::~AvMedium() noexcept
{
    av_dict_free(&m_options);
    avformat_free_context(m_format_ctx);
}

auto AvMedium::checkUrl() noexcept -> void
{
    auto urlStr{boost::urls::parse_uri(m_url)};
    if (!urlStr.has_value())
    {
        return;
    }
    if (std::string{urlStr.value().scheme()} == std::string{"rtsp"})
    {
        m_urlFormat = UrlFormat::RTSP;
    }
    else if (std::string{urlStr.value().scheme()} == std::string{"rtmp"})
    {
        m_urlFormat = UrlFormat::RTMP;
    }
    else if (std::string{urlStr.value().scheme()} == std::string{"udp"})
    {
        m_urlFormat = UrlFormat::UDP;
    }
    else
    {
        m_urlFormat = UrlFormat::OTHER;
    }
}

auto AvMedium::avideoHandle() noexcept -> void
{
    if (!std::invoke(&AvMedium::avOpenInput, this))
    {
        return;
    }
    if (!std::invoke(&AvMedium::findVideoStream, this))
    {
        return;
    }
    if (!std::invoke(&AvMedium::initCodecContext, this))
    {
        return;
    }
}

auto AvMedium::smuSetOptions() noexcept -> void
{
    std::vector<std::pair<const char*, const char*>> optionsMap{
        {"buffer_size", "32768"},      // 网络缓冲大小，三协议均有效，UDP通常可适当调小
        {"stimeout", "5000000"},       // 网络连接和读超时（微秒）
        {"fflags", "nobuffer"},        // 禁用内部缓冲，减少延迟
        {"flush_packets", "1"},        // 每包立即处理，减少延迟
        {"analyzeduration", "0"},      // 禁止流分析，快速启动
        {"framedrop", "1"},            // 丢帧防止阻塞，实时性关键
        {"reconnect", "1"},            // 断线自动重连
        {"reconnect_at_eof", "1"},     // 流结束自动重连
        {"reconnect_streamed", "1"},   // 指定网络流
        {"reconnect_delay_max", "5"},  // 最大重连间隔秒
        {"avioflags", "8"},            // 非阻塞IO
        {"flags", "low_delay"},        // 编解码低延迟标志

    };
    for (const auto& [__key, __value] : optionsMap)
    {
        av_dict_set(&m_options, __key, __value, 0);
    }
}

auto AvMedium::rtspSetOptions() noexcept -> void
{
    std::vector<std::pair<const char*, const char*>> optionsMap{
        {"rtsp_transport", "tcp"},    // RTSP专用，指定传输协议
        {"reorder_queue_size", "0"},  // 禁用帧重排序，RTSP流中B帧多时有效
        {"seekable", "0"},            // 禁用seek，直播流常用
        {"err_detect", "none"},       // 错误检测关闭，容错性强
        {"explode", "0"},
        {"buffer", "0"},
        {"careful", "0"},
        {"compliant", "0"},
        {"aggressive", "0"},
    };

    for (const auto& [__key, __value] : optionsMap)
    {
        av_dict_set(&m_options, __key, __value, 0);
    }
}

auto AvMedium::rtmpSetOptions() noexcept -> void
{
    std::vector<std::pair<const char*, const char*>> optionsMap{
        {"rtmp_tcp_nodelay", "1"},  // RTMP专用，禁用Nagle算法，减少延迟
        {"rtmp_buffer", "32768"},   // RTMP专用缓冲大小
    };
    for (const auto& [__key, __value] : optionsMap)
    {
        av_dict_set(&m_options, __key, __value, 0);
    }
}

auto AvMedium::udpSetOptions() noexcept -> void
{
    std::vector<std::pair<const char*, const char*>> optionsMap{};
    for (const auto& [__key, __value] : optionsMap)
    {
        av_dict_set(&m_options, __key, __value, 0);
    }
}

auto AvMedium::avOpenInput() noexcept -> bool
{
    if (m_urlFormat == UrlFormat::RTSP || m_urlFormat == UrlFormat::RTMP || m_urlFormat == UrlFormat::UDP)
    {
        if (m_urlFormat == UrlFormat::RTSP)
        {
            this->rtspSetOptions();
        }
        else if (m_urlFormat == UrlFormat::RTMP)
        {
            this->rtmpSetOptions();
        }
        else if (m_urlFormat == UrlFormat::UDP)
        {
            this->udpSetOptions();
        }
        this->smuSetOptions();
    }
    if (avformat_open_input(&m_format_ctx, m_url.c_str(), nullptr, &m_options) < 0)
    {
        return false;
    }
    return true;
}

auto AvMedium::findVideoStream() noexcept -> bool
{
    if (avformat_find_stream_info(m_format_ctx, nullptr) < 0)
    {
        return false;
    }
    m_video_index = av_find_best_stream(m_format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_video_index < 0)
    {
        return false;
    }
    return true;
}

auto AvMedium::initCodecContext() noexcept -> bool
{
    // 从视频流中提取编解码参数（如编码格式、分辨率、帧率等）
    AVCodecParameters* codec_parameters{m_format_ctx->streams[m_video_index]->codecpar};
    m_codec_ctx = avcodec_alloc_context3(nullptr);
    if (!m_codec_ctx)
    {
        return false;
    }
    if (avcodec_parameters_to_context(m_codec_ctx, codec_parameters) < 0)
    {
        return false;
    }
    m_codec_ctx->codec_id     = AV_CODEC_ID_H264;  // 指定使用的编码器为 H.264
    m_codec_ctx->thread_count = 8;                 // 设置编码时使用的线程数为 8
    const AVCodec* codec{avcodec_find_decoder(codec_parameters->codec_id)};
    if (!codec)
    {
        return false;
    }
    if (avcodec_open2(m_codec_ctx, codec, nullptr) < 0)
    {
        return false;
    }
    return true;
}

#define OPENCV
// #define SDL2

#ifdef OPENCV
// SWS_BICUBIC 双三次插值
// SWS_LANCZOS 高清图片
// SWS_FAST_BILINEAR 低延迟低质量图片
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
