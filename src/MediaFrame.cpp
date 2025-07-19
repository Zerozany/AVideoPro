#include "MediaFrame.h"

#include <boost/url.hpp>

MediaFrame::MediaFrame()
{
    av_log_set_level(AV_LOG_WARNING);
    std::invoke(&MediaFrame::connectSignalToSlot, this);
}

MediaFrame::~MediaFrame() noexcept
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

auto MediaFrame::getUrl() const noexcept -> std::string
{
    return this->m_url;
}

auto MediaFrame::setUrl(const std::string& _url) noexcept -> void
{
    if (m_url == _url)
    {
        return;
    }
    m_url = _url;
    Q_EMIT this->urlChanged();
}

auto MediaFrame::getUrlType() const noexcept -> UrlType
{
    return this->m_urlType;
}

auto MediaFrame::setUrlType(const UrlType& _urlType) noexcept -> void
{
    if (m_urlType == _urlType)
    {
        return;
    }
    m_urlType = _urlType;
    Q_EMIT this->urlTypeChanged();
}

auto MediaFrame::connectSignalToSlot() noexcept -> void
{
    connect(this, &MediaFrame::urlChanged, this, &MediaFrame::onUrlChanged);
    connect(this, &MediaFrame::urlTypeChanged, this, &MediaFrame::onUrlTypeChanged);
}

auto MediaFrame::onUrlChanged() noexcept -> void
{
    if (m_frameHandle.load())
    {
        m_frameHandle.store(false);
    }
    auto urlStr{boost::urls::parse_uri(m_url)};
    if (!urlStr.has_value())
    {
        return;
    }
    if (std::string{urlStr.value().scheme()} == std::string{"rtsp"})
    {
        this->setUrlType(MediaFrame::UrlType::RTSP);
    }
    else if (std::string{urlStr.value().scheme()} == std::string{"rtmp"})
    {
        this->setUrlType(MediaFrame::UrlType::RTMP);
    }
    else if (std::string{urlStr.value().scheme()} == std::string{"udp"})
    {
        this->setUrlType(MediaFrame::UrlType::UDP);
    }
    if (!std::invoke(&MediaFrame::mediaOpenInput, this))
    {
        return;
    }
    if (!std::invoke(&MediaFrame::findVideoStream, this))
    {
        return;
    }
    if (!std::invoke(&MediaFrame::codecContext, this))
    {
        return;
    }
    m_frameHandle.store(true);
}

auto MediaFrame::onUrlTypeChanged() noexcept -> void
{
    auto setOptions{[this](const std::map<const char*, const char*>& _map) {
        if (m_frameHandle.load())
        {
            av_dict_free(&m_options);
        }
        for (const auto& [__key, __value] : _map)
        {
            av_dict_set(&m_options, __key, __value, 0);
        }
    }};

    switch (m_urlType)
    {
        case UrlType::RTSP:
        {
            std::invoke(setOptions, rtspOptionsMap);
            break;
        }
        case UrlType::RTMP:
        {
            std::invoke(setOptions, rtmpOptionsMap);
            break;
        }
        case UrlType::UDP:
        {
            std::invoke(setOptions, udpOptionsMap);
            break;
        }
        case UrlType::OTHER:
        {
            break;
        }
        default:
        {
            std::unreachable();
        }
    }
}

auto MediaFrame::flushPacket() noexcept -> Generator<AVFrame>
{
    if (!m_packet)
    {
        m_packet = av_packet_alloc();
    }
    if (!m_frame)
    {
        m_frame = av_frame_alloc();
    }
    while (av_read_frame(m_formatCtx, m_packet) >= 0)
    {
        if (!m_frameHandle.load())
        {
            co_return;
        }
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
        if (ret == AVERROR(EAGAIN))
        {
            av_frame_unref(m_frame);
            continue;
        }
        else if (ret == AVERROR_EOF)
        {
            co_return;
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
            av_frame_unref(m_frame);
        }
    }
}

auto MediaFrame::getMediaState() noexcept -> bool
{
    return this->m_frameHandle;
}

auto MediaFrame::mediaOpenInput() noexcept -> bool
{
    m_formatCtx = avformat_alloc_context();
    if (avformat_open_input(&m_formatCtx, m_url.c_str(), nullptr, &m_options) < 0)
    {
        return false;
    }
    return true;
}

auto MediaFrame::findVideoStream() noexcept -> bool
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

auto MediaFrame::codecContext() noexcept -> bool
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
    if (srcFmt == AV_PIX_FMT_NONE)
    {
        return false;
    }
    m_swsCtx = sws_getContext(
        m_codecCtx->width, m_codecCtx->height, srcFmt,
        m_codecCtx->width, m_codecCtx->height, AV_PIX_FMT_BGR24,
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
