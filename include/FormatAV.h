_Pragma("once");

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

}
#include <functional>
#include <iostream>
#include <ranges>
#include <string>

class FormatAV
{
public:
    explicit(true) FormatAV(const std::string& _url);
    ~FormatAV() noexcept;

public:
    auto read() noexcept -> void;

private:
    auto av_init() noexcept -> bool;

private:
    std::string      m_url{};
    AVFormatContext* m_format_ctx{avformat_alloc_context()};
    AVCodecContext*  m_codec_ctx{nullptr};
    int              m_video_index{};
    AVFrame*         m_frame{av_frame_alloc()};
    AVPacket*        m_packet{av_packet_alloc()};
    SwsContext*      sws_ctx{nullptr};
};
