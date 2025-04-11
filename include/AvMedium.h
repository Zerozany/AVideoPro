_Pragma("once");

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <string>

class AvMedium
{
public:
    explicit(true) AvMedium(const std::string& _url);
    ~AvMedium() noexcept;

public:
    auto read() noexcept -> void;

private:
    auto av_init() noexcept -> bool;

    auto set_dict(std::string _type) noexcept -> void;

private:
    std::string      m_url{};
    AVDictionary*    m_options{nullptr};
    AVFormatContext* m_format_ctx{avformat_alloc_context()};
    AVCodecContext*  m_codec_ctx{nullptr};
    int              m_video_index{};
    AVFrame*         m_frame{av_frame_alloc()};
    AVPacket*        m_packet{av_packet_alloc()};
    SwsContext*      m_sws_ctx{nullptr};
};
