_Pragma("once");

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <string>

class AvMedium
{
public:
    enum struct UrlFormat
    {
        OTHER = 0x00,
        RTSP  = 0x01,
        RTMP  = 0x02,
        UDP   = 0x03,
    };

public:
    explicit(true) AvMedium(const std::string& _url);
    ~AvMedium() noexcept;

public:
    auto read() noexcept -> void;

private:
    auto checkUrl() noexcept -> void;

private:
    auto avideoHandle() noexcept -> void;

    auto avOpenInput() noexcept -> bool;

    auto findVideoStream() noexcept -> bool;

    auto initCodecContext() noexcept -> bool;

    auto rtspSetOptions() noexcept -> void;

    auto rtmpSetOptions() noexcept -> void;

    auto udpSetOptions() noexcept -> void;

    auto smuSetOptions() noexcept -> void;

private:
    std::string      m_url{};
    UrlFormat        m_urlFormat{};
    AVDictionary*    m_options{nullptr};
    AVFormatContext* m_format_ctx{avformat_alloc_context()};
    AVCodecContext*  m_codec_ctx{nullptr};
    int              m_video_index{};
    AVFrame*         m_frame{av_frame_alloc()};
    AVPacket*        m_packet{av_packet_alloc()};
    SwsContext*      m_sws_ctx{nullptr};
};
