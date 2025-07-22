#include "MediaFrame.h"

#include <boost/url.hpp>
#include <iostream>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
}

MediaFrame::MediaFrame(const std::string& _url)
{
    av_log_set_level(AV_LOG_DEBUG);
    std::invoke(&MediaFrame::setStreamUrl, this, _url);
}

MediaFrame::MediaFrame()
{
    av_log_set_level(AV_LOG_DEBUG);
}

MediaFrame::~MediaFrame() noexcept
{
    std::invoke(&MediaFrame::stop, this);
    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
    }
    if (m_codecContext)
    {
        avcodec_free_context(&m_codecContext);
    }
    if (m_formatContext)
    {
        avformat_close_input(&m_formatContext);
    }
    if (m_options)
    {
        av_dict_free(&m_options);
    }
}

auto MediaFrame::start() noexcept -> bool
{
    std::invoke(&MediaFrame::setDictOptions, this);
    if (!std::invoke(&MediaFrame::initMedia, this))
    {
        return false;
    }
    return true;
}

auto MediaFrame::flushPacket() noexcept -> Generator<ImageData>
{
    AVPacket* packet{av_packet_alloc()};
    AVFrame*  frame{av_frame_alloc()};
    while (m_frameHandle.load())
    {
        if (av_read_frame(m_formatContext, packet) < 0)
        {
            av_packet_unref(packet);
            continue;
        }
        if (packet->stream_index != m_videoIndex)
        {
            av_packet_unref(packet);
            continue;
        }
        if (avcodec_send_packet(m_codecContext, packet) < 0)
        {
            av_packet_unref(packet);
            continue;
        }
        int ret{avcodec_receive_frame(m_codecContext, frame)};
        if (ret == AVERROR(EAGAIN))
        {
            av_frame_unref(frame);
            continue;
        }
        else if (ret == AVERROR_EOF)
        {
            if (frame)
            {
                av_frame_free(&frame);
            }
            if (packet)
            {
                av_packet_free(&packet);
            }
            co_return;
        }
        else if (ret < 0)
        {
            av_frame_unref(frame);
            continue;
        }
        av_packet_unref(packet);
        if (frame)
        {
            int                  numBytes{av_image_get_buffer_size(AV_PIX_FMT_BGR24, frame->width, frame->height, 1)};
            std::vector<uint8_t> rgbBuffer(numBytes);
            uint8_t*             dest[4]{rgbBuffer.data(), nullptr, nullptr, nullptr};
            int                  lineSize[4]{3 * frame->width, 0, 0, 0};
            sws_scale(m_swsCtx, frame->data, frame->linesize, 0, frame->height, dest, lineSize);
            ImageData imageData{
                .rgbBuffer{rgbBuffer},
                .width{frame->width},
                .height{frame->height},
                .lineSize{lineSize[0]},
            };
            co_yield imageData;
            av_frame_unref(frame);
        }
    }
    if (frame)
    {
        av_frame_free(&frame);
    }
    if (packet)
    {
        av_packet_free(&packet);
    }
}

auto MediaFrame::stop() noexcept -> void
{
    if (m_frameHandle.load())
    {
        m_frameHandle.store(false);
    }
}

auto MediaFrame::setStreamUrl(const std::string& _url) noexcept -> void
{
    auto streamUrl{boost::urls::parse_uri(_url)};
    if (!streamUrl.has_value())
    {
        return;
    }
    if (std::string{streamUrl.value().scheme()} == std::string{"rtsp"})
    {
        this->m_urlHeader = UrlHeader::RTSP;
    }
    else if (std::string{streamUrl.value().scheme()} == std::string{"rtmp"})
    {
        this->m_urlHeader = UrlHeader::RTMP;
    }
    else
    {
        this->m_urlHeader = UrlHeader::OTHER;
    }
    m_url = _url;
}

auto MediaFrame::setDictOptions() noexcept -> void
{
    switch (m_urlHeader)
    {
        case UrlHeader::OTHER:
        {
            break;
        }
        case UrlHeader::RTSP:
        {
            break;
        }
        case UrlHeader::RTMP:
        {
            break;
        }
        default:
        {
            std::unreachable();
        }
    }
}

auto MediaFrame::initMedia() noexcept -> bool
{
    if (m_frameHandle.load())
    {
        return false;
    }
    /// @brief 打开多媒体文件
    if (avformat_open_input(&m_formatContext, m_url.data(), nullptr, nullptr) < 0)
    {
        return false;
    }
    /// @brief 解析媒体文件或流的头部及部分数据
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0)
    {
        return false;
    }
    /// @brief 从多媒体文件中找到视频流
    m_videoIndex = av_find_best_stream(m_formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_videoIndex < 0 && m_videoIndex < m_formatContext->nb_streams)
    {
        return false;
    }
    /// @brief 获取当前视频流的编解码参数
    AVCodecParameters* codecParameters{m_formatContext->streams[m_videoIndex]->codecpar};
    /// @brief 获取解码器
    const AVCodec* codec{avcodec_find_decoder(codecParameters->codec_id)};
    if (!codec)
    {
        return false;
    }
    /// @brief 把输入流的编码参数正确加载到解码器上下文
    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext)
    {
        return false;
    }
    // 设置编码器参数
    m_codecContext->width        = codecParameters->width;
    m_codecContext->height       = codecParameters->height;
    m_codecContext->bit_rate     = 500000;              // 比特率为 500000（单位是比特每秒）
    m_codecContext->time_base    = AVRational{1, 30};   // 时间基准，表示帧间隔，这里是 1/30，即每秒30帧
    m_codecContext->framerate    = AVRational{30, 1};   // 帧率，也设置为 30fps
    m_codecContext->gop_size     = 10;                  // 关键帧间隔（Group Of Pictures），这里是10，即每10帧一个关键帧
    m_codecContext->max_b_frames = 0;                   // 最大B帧数，设置为0
    m_codecContext->pix_fmt      = AV_PIX_FMT_YUV420P;  // 像素格式，YUV420P
    m_codecContext->codec_id     = AV_CODEC_ID_H264;    // 指定使用的编码器为 H.264
    m_codecContext->thread_count = 8;                   // 设置编码时使用的线程数为 8
    if (codec->id == AV_CODEC_ID_H264)
    {
        av_opt_set(m_codecContext->priv_data, "preset", "ultrafast", 0);
        av_opt_set(m_codecContext->priv_data, "tune", "zerolatency", 0);
    }
    /// @brief 将媒体流中的编码参数拷贝到解码器上下文
    if (avcodec_parameters_to_context(m_codecContext, codecParameters) < 0)
    {
        return false;
    }
    /// @brief 初始化解码器上下文并打开指定的解码器
    if (avcodec_open2(m_codecContext, codec, nullptr) < 0)
    {
        return false;
    }
    /// @brief 图像像素格式转换与缩放上下文的初始化或复用
    m_swsCtx = sws_getCachedContext(
        nullptr, m_codecContext->width, m_codecContext->height,
        AV_PIX_FMT_YUV420P, m_codecContext->width, m_codecContext->height,
        AV_PIX_FMT_BGR24, SWS_BICUBIC, nullptr, nullptr, nullptr);
    // SWS_BICUBIC 双三次插值
    // SWS_LANCZOS 高清图片
    // SWS_FAST_BILINEAR 低延迟低质量图片
    if (!m_swsCtx)
    {
        return false;
    }
    m_frameHandle.store(true);
    return true;
}
