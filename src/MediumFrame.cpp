#include "MediumFrame.h"

#include <spdlog/spdlog.h>

#include <boost/url.hpp>

extern "C" {
#include <libavutil/avutil.h>
}

MediumFrame::MediumFrame(QObject* _parent) : QObject{_parent}
{
    av_log_set_level(AV_LOG_WARNING);
    std::invoke(&MediumFrame::connectSignalToSlot, this);
}

MediumFrame::MediumFrame(const std::string& _url, QObject* _parent)
    : QObject{_parent}, m_url{_url}
{
    av_log_set_level(AV_LOG_WARNING);
    std::invoke(&MediumFrame::connectSignalToSlot, this);
    this->setUrl(m_url);
}

MediumFrame::~MediumFrame() noexcept
{
    std::invoke(&MediumFrame::clearMedia, this);
}

auto MediumFrame::getUrl() const noexcept -> std::string
{
    return this->m_url;
}

auto MediumFrame::setUrl(const std::string& _url) noexcept -> void
{
    if (m_url == _url)
    {
        return;
    }
    m_url = _url;
    Q_EMIT this->urlChanged();
}

auto MediumFrame::getUrlFormat() const noexcept -> UrlFormat
{
    return this->m_urlFormat;
}

auto MediumFrame::setUrlFormat(const UrlFormat& _urlFormat) noexcept -> void
{
    if (m_urlFormat == _urlFormat)
    {
        return;
    }
    m_urlFormat = _urlFormat;
    Q_EMIT this->onUrlChanged();
}

auto MediumFrame::connectSignalToSlot() noexcept -> void
{
    connect(this, &MediumFrame::urlChanged, this, &MediumFrame::onUrlChanged);
    connect(this, &MediumFrame::urlFormatChanged, this, &MediumFrame::onUrlFormatChanged);
}

auto MediumFrame::mediumStart() noexcept -> void
{
    if (!std::invoke(&MediumFrame::avOpenInput, this))
    {
        spdlog::error("AVFormat open input error");
        return;
    }
    if (!std::invoke(&MediumFrame::findVideoStream, this))
    {
        spdlog::error("AVFormat find stream infomation error");
        return;
    }
    if (!std::invoke(&MediumFrame::initCodecContext, this))
    {
        spdlog::error("AVideo Codec context error");
        return;
    }
    m_frameHandle = true;
    spdlog::info("Media live streaming has been activated");
}

auto MediumFrame::getFrameState() noexcept -> bool
{
    return this->m_frameHandle;
}

auto MediumFrame::avOpenInput() noexcept -> bool
{
    m_formatCtx = avformat_alloc_context();
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
    if (!m_packet)
    {
        m_packet = av_packet_alloc();
    }
    if (!m_frame)
    {
        m_frame = av_frame_alloc();
    }
    while (av_read_frame(m_formatCtx, m_packet) >= 0 && m_frameHandle)
    {
        if (m_packet->stream_index != m_videoIndex)
        {
            av_packet_unref(m_packet);
            continue;
        }
        if (avcodec_send_packet(m_codecCtx, m_packet) < 0)
        {
            av_packet_unref(m_packet);
            continue;
        }
        int ret{avcodec_receive_frame(m_codecCtx, m_frame)};
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_frame_unref(m_frame);
            continue;
        }
        else if (ret < 0)
        {
            av_frame_unref(m_frame);
            continue;
        }
        av_packet_unref(m_packet);
        if (m_frame)
        {
            co_yield m_frame;
        }
        av_frame_unref(m_frame);
    }
}

auto MediumFrame::clearMedia() noexcept -> void
{
    if (m_formatCtx)
    {
        avformat_close_input(&m_formatCtx);
    }
    if (m_codecCtx)
    {
        avcodec_free_context(&m_codecCtx);
    }
    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
    }
    if (m_packet)
    {
        av_packet_free(&m_packet);
    }
    if (m_frame)
    {
        av_frame_free(&m_frame);
    }
    if (m_options)
    {
        av_dict_free(&m_options);
    }
}

void MediumFrame::onUrlChanged()
{
    m_frameHandle = false;
    m_formatCtx   = nullptr;
    m_codecCtx    = nullptr;
    m_swsCtx      = nullptr;
    m_packet      = nullptr;
    m_frame       = nullptr;
    m_options     = nullptr;
    auto urlStr{boost::urls::parse_uri(m_url)};
    if (!urlStr.has_value())
    {
        return;
    }
    if (std::string{urlStr.value().scheme()} == std::string{"rtsp"})
    {
        this->setUrlFormat(UrlFormat::RTSP);
    }
    else if (std::string{urlStr.value().scheme()} == std::string{"rtmp"})
    {
        this->setUrlFormat(UrlFormat::RTMP);
    }
    else if (std::string{urlStr.value().scheme()} == std::string{"udp"})
    {
        this->setUrlFormat(UrlFormat::UDP);
    }
    else
    {
        this->setUrlFormat(UrlFormat::OTHER);
    }
}

void MediumFrame::onUrlFormatChanged()
{
    std::map<const char*, const char*> smuOptionsMap{
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

    std::map<const char*, const char*> rtspOptionsMap{
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

    std::map<const char*, const char*> rtmpOptionsMap{
        {"rtmp_tcp_nodelay", "1"},  // RTMP专用，禁用Nagle算法，减少延迟
        {"rtmp_buffer", "32768"},   // RTMP专用缓冲大小
    };

    std::map<const char*, const char*> udpOptionsMap{};

    auto setOptions{[this](const std::map<const char*, const char*>& _map) {
        for (const auto& [__key, __value] : _map)
        {
            av_dict_set(&m_options, __key, __value, 0);
        }
    }};

    if (m_urlFormat == UrlFormat::RTSP || m_urlFormat == UrlFormat::RTMP || m_urlFormat == UrlFormat::UDP)
    {
        std::invoke(setOptions, smuOptionsMap);
        if (m_urlFormat == UrlFormat::RTSP)
        {
            std::invoke(setOptions, rtspOptionsMap);
        }
        else if (m_urlFormat == UrlFormat::RTMP)
        {
            std::invoke(setOptions, rtmpOptionsMap);
        }
        else if (m_urlFormat == UrlFormat::UDP)
        {
            std::invoke(setOptions, udpOptionsMap);
        }
    }
}
