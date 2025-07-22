_Pragma("once");
#include <atomic>
#include <string>
#include <vector>

#include "Generator.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

struct ImageData
{
    std::vector<uint8_t> rgbBuffer{};
    int                  width{};
    int                  height{};
    int                  lineSize{};
};

class MediaFrame
{
public:
    enum struct UrlHeader
    {
        OTHER = 0x00,
        RTSP  = 0x01,
        RTMP  = 0x02,
#if false
        UDP   = 0x03,
        HTTP  = 0x04,
        HTTPS = 0x05,
#endif
    };

public:
    explicit(true) MediaFrame(const std::string& _url);
    explicit(true) MediaFrame();
    ~MediaFrame() noexcept;

    auto setStreamUrl(const std::string& _url) noexcept -> void;

    auto start() noexcept -> bool;

    auto flushPacket() noexcept -> Generator<ImageData>;

    auto stop() noexcept -> void;

private:
    auto setDictOptions() noexcept -> void;

    auto initMedia() noexcept -> bool;

private:
    std::string       m_url{};
    UrlHeader         m_urlHeader{};
    AVDictionary*     m_options{nullptr};
    AVFormatContext*  m_formatContext{nullptr};
    int               m_videoIndex{};
    AVCodecContext*   m_codecContext{nullptr};
    SwsContext*       m_swsCtx{nullptr};
    std::atomic<bool> m_frameHandle{false};
};
