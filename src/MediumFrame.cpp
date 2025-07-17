#include "MediumFrame.h"

#include <boost/url.hpp>

extern "C" {
#include <libavutil/avutil.h>
}

MediumFrame::MediumFrame()
{
    av_log_set_level(AV_LOG_WARNING);
}

MediumFrame::MediumFrame(const std::string& _url) : m_url{_url}
{
    av_log_set_level(AV_LOG_WARNING);
    std::invoke(&MediumFrame::setStreamUrl, this, m_url);
}

MediumFrame::~MediumFrame() noexcept
{
    avcodec_free_context(&m_codecCtx);
    avformat_close_input(&m_formatCtx);
    av_packet_free(&m_packet);
    av_frame_free(&m_frame);
    av_dict_free(&m_options);
    sws_freeContext(m_swsCtx);
}

auto MediumFrame::setStreamUrl(const std::string& _url) noexcept -> void
{
    if (m_url == _url) [[unlikely]]
    {
        return;
    }
    m_url = _url;
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

auto MediumFrame::mediumStart() noexcept -> void
{
    if (!std::invoke(&MediumFrame::avOpenInput, this))
    {
        return;
    }
    if (!std::invoke(&MediumFrame::findVideoStream, this))
    {
        return;
    }
    if (!std::invoke(&MediumFrame::initCodecContext, this))
    {
        return;
    }
}

auto MediumFrame::smuSetOptions() noexcept -> void
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

auto MediumFrame::rtspSetOptions() noexcept -> void
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

auto MediumFrame::rtmpSetOptions() noexcept -> void
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

auto MediumFrame::udpSetOptions() noexcept -> void
{
    std::vector<std::pair<const char*, const char*>> optionsMap{};
    for (const auto& [__key, __value] : optionsMap)
    {
        av_dict_set(&m_options, __key, __value, 0);
    }
}

auto MediumFrame::avOpenInput() noexcept -> bool
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
    if (avformat_open_input(&m_formatCtx, m_url.c_str(), nullptr, &m_options) < 0)
    {
        return false;
    }
    return true;
}

auto MediumFrame::findVideoStream() noexcept -> bool
{
    if (avformat_find_stream_info(m_formatCtx, nullptr) < 0)
    {
        return false;
    }
    m_videoIndex = av_find_best_stream(m_formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoIndex < 0)
    {
        return false;
    }
    return true;
}

auto MediumFrame::initCodecContext() noexcept -> bool
{
    AVCodecParameters* codecParameters{m_formatCtx->streams[m_videoIndex]->codecpar};
    m_codecCtx = avcodec_alloc_context3(nullptr);
    if (!m_codecCtx)
    {
        return false;
    }
    if (avcodec_parameters_to_context(m_codecCtx, codecParameters) < 0)
    {
        return false;
    }
    m_codecCtx->codec_id     = AV_CODEC_ID_H264;  // 指定使用的编码器为 H.264
    m_codecCtx->thread_count = 8;                 // 设置编码时使用的线程数为 8
    const AVCodec* codec{avcodec_find_decoder(codecParameters->codec_id)};
    if (!codec)
    {
        return false;
    }
    if (avcodec_open2(m_codecCtx, codec, nullptr) < 0)
    {
        return false;
    }
    AVPixelFormat srcFmt{static_cast<AVPixelFormat>(m_codecCtx->pix_fmt)};
    int           srcW{m_codecCtx->width};
    int           srcH{m_codecCtx->height};
    if (srcFmt == AV_PIX_FMT_NONE)
    {
        return false;
    }
    m_swsCtx = sws_getContext(
        srcW, srcH, srcFmt,
        srcW, srcH, AV_PIX_FMT_BGR24,
        SWS_LANCZOS, nullptr, nullptr, nullptr);
    // SWS_BICUBIC 双三次插值
    // SWS_LANCZOS 高清图片
    // SWS_FAST_BILINEAR 低延迟低质量图片
    if (!m_swsCtx)
    {
        return false;
    }
    return true;
}

auto MediumFrame::flushPacket() noexcept -> MediumFrameGenerator
{
    AVFrame* latestFrame{nullptr};
    while (av_read_frame(m_formatCtx, m_packet) >= 0)
    {
        if (m_packet->stream_index != m_videoIndex)
        {
            continue;
        }
        if (avcodec_send_packet(m_codecCtx, m_packet) < 0)
        {
            continue;
        }
        while (true)
        {
            AVFrame* tmpFrame{av_frame_alloc()};
            int      ret{avcodec_receive_frame(m_codecCtx, tmpFrame)};
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                av_frame_free(&tmpFrame);
                break;
            }
            else if (ret < 0)
            {
                av_frame_free(&tmpFrame);
                break;
            }
            if (latestFrame)
            {
                av_frame_unref(latestFrame);
                av_frame_free(&latestFrame);
            }
            latestFrame = tmpFrame;
        }
        av_packet_unref(m_packet);
        if (latestFrame)
        {
            co_yield latestFrame;
            latestFrame = nullptr;
        }
    }
    // 解码完毕，释放最新帧
    if (latestFrame)
    {
        av_frame_unref(latestFrame);
        av_frame_free(&latestFrame);
    }
}
