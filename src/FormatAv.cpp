#include "FormatAV.h"

FormatAV::FormatAV(const std::string& _url) : m_url{_url}
{
    std::invoke(av_log_set_level, AV_LOG_DEBUG);
    if (!std::invoke(&FormatAV::av_init, this))
    {
        return;
    }
}

FormatAV::~FormatAV() noexcept
{
    avformat_free_context(m_format_ctx);
}

auto FormatAV::read() noexcept -> void
{
    int frame_count = 0;  // 用于生成唯一的文件名

    while (av_read_frame(m_format_ctx, m_packet) >= 0)
    {
        if (m_packet->stream_index == m_video_index)
        {
            // 发送数据包到解码器
            if (avcodec_send_packet(m_codec_ctx, m_packet) == 0)
            {
                // 接收解码后的帧
                while (avcodec_receive_frame(m_codec_ctx, m_frame) == 0)
                {
                }
            }
        }
        // 释放数据包
        av_packet_unref(m_packet);
    }
}

auto FormatAV::av_init() noexcept -> bool
{
    if (avformat_open_input(&m_format_ctx, m_url.c_str(), nullptr, nullptr) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "The input address is invalid:%s\n", m_url.data());
        return false;
    }
    av_log(nullptr, AV_LOG_DEBUG, "The input address is:%s\n", m_format_ctx->url);

    // std::cout << "Format: " << m_format_ctx->iformat->name << std::endl;
    // std::cout << "Number of streams: " << m_format_ctx->nb_streams << std::endl;
    // std::cout << "Duration: " << m_format_ctx->duration << " us" << std::endl;

    if (avformat_find_stream_info(m_format_ctx, nullptr) < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Could not find stream\n");
        return false;
    }
    // for (unsigned int i = 0; i < m_format_ctx->nb_streams; i++)
    // {
    //     AVStream* stream = m_format_ctx->streams[i];
    //     std::cout << "Stream " << i << ": "
    //               << "codec_type = " << stream->codecpar->codec_type
    //               << ", codec_id = " << stream->codecpar->codec_id
    //               << ", bit_rate = " << stream->codecpar->bit_rate
    //               << ", time_base = " << stream->time_base.num << "/" << stream->time_base.den
    //               << ", r_frame_rate = " << stream->r_frame_rate.num << "/" << stream->r_frame_rate.den
    //               << std::endl;
    // }

    m_video_index = av_find_best_stream(m_format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_video_index < 0)
    {
        av_log(nullptr, AV_LOG_ERROR, "Could not find vidoe stream\n");
        return false;
    }
    // 从视频流中提取编解码参数（如编码格式、分辨率、帧率等）
    AVCodecParameters* codec_parameters{m_format_ctx->streams[m_video_index]->codecpar};
    std::cout << codec_parameters->width << '\n';
    std::cout << codec_parameters->height << '\n';
    std::cout << static_cast<double>(codec_parameters->framerate.num) / codec_parameters->framerate.den << '\n';

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
    // sws_ctx = sws_getContext(
    //     m_codec_ctx->width, m_codec_ctx->height, m_codec_ctx->pix_fmt,
    //     m_codec_ctx->width, m_codec_ctx->height, AV_PIX_FMT_YUVJ420P,
    //     SWS_BILINEAR, nullptr, nullptr, nullptr);
    // if (!sws_ctx)
    // {
    //     av_log(nullptr, AV_LOG_ERROR, "Failed to create SwsContext\n");
    //     return;
    // }
    return true;
}
