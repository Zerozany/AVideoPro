#include "FormatAV.h"

#include <functional>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <ranges>

FormatAV::FormatAV(const std::string& _url) : m_url{_url}
{
    std::invoke(av_log_set_level, AV_LOG_DEBUG);
    std::invoke(&FormatAV::set_dict, this, m_url);
    if (!std::invoke(&FormatAV::av_init, this))
    {
        return;
    }
}

FormatAV::~FormatAV() noexcept
{
    av_dict_free(&m_options);
    avformat_free_context(m_format_ctx);
}

auto FormatAV::read() noexcept -> void
{
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
        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    // SWS_BICUBIC 双三次插值
    // SWS_LANCZOS 高清图片

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

auto FormatAV::set_dict(std::string _type) noexcept -> void
{
    if (m_url.find("rtsp://") == 0) [[likely]]
    {
        // 强制使用 TCP
        av_dict_set(&m_options, "rtsp_transport", "tcp", 0);

        // 5秒超时
        av_dict_set(&m_options, "stimeout", "5000000", 0);

        // 设置分析持续时间为较小的值（例如 1000000 微秒，即 1 秒）
        av_dict_set(&m_options, "analyzeduration", "1000000", 0);

        // 设置探测大小为较小的值（例如 500000 字节）
        av_dict_set(&m_options, "probesize", "500000", 0);

        // 设置最大延迟（例如，100ms）
        av_dict_set(&m_options, "max_delay", "100", 0);

        // 禁用内部缓冲，减少延迟
        av_dict_set(&m_options, "fflags", "nobuffer", 0);

        // 选择 GPU 设备
        av_dict_set(&m_options, "hwaccel_device", "0", 0);

        // 降低帧率
        av_dict_set(&m_options, "r", "30", 0);
    }
    else if (m_url.find("http://") == 0 || m_url.find("https://") == 0)
    {
        // 5秒超时
        av_dict_set(&m_options, "stimeout", "5000000", 0);
    }
}

auto FormatAV::av_init() noexcept -> bool
{
    if (avformat_open_input(&m_format_ctx, m_url.c_str(), nullptr, &m_options) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "The input address is invalid:%s\n", m_url.data());
        return false;
    }
    // av_log(nullptr, AV_LOG_DEBUG, "The input address is:%s\n", m_format_ctx->url);
    av_log(nullptr, AV_LOG_DEBUG, "Format:%s\n", m_format_ctx->iformat->name);
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
        av_log(nullptr, AV_LOG_ERROR, "Could not find stream\n");
        return false;
    }

    m_video_index = av_find_best_stream(m_format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_video_index < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Could not find vidoe stream\n");
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
        av_log(nullptr, AV_LOG_ERROR, "Could not allocate codec context\n");
        return false;
    }

    if (avcodec_parameters_to_context(m_codec_ctx, codec_parameters) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Could not copy codec parameters to context\n");
        return false;
    }

    const AVCodec* codec{avcodec_find_decoder(codec_parameters->codec_id)};
    if (!codec)
    {
        av_log(nullptr, AV_LOG_ERROR, "Could supported codec\n");
        return false;
    }
    if (avcodec_open2(m_codec_ctx, codec, nullptr) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Could not allocate codec context\n");
        return false;
    }
#endif
    return true;
}
