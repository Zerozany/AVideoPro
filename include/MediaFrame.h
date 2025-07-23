_Pragma("once");
#include <atomic>
#include <map>
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

inline std::map<const char*, const char*> rtmpDictMap{
    {"rtmp_tcp_nodelay", "1"},             // RTMP专用，禁用Nagle算法，减少延迟
    {"rtmp_buffer", "32768"},              // RTMP专用缓冲大小
    {"buffer_size", "32768"},              // 网络缓冲大小，三协议均有效，UDP通常可适当调小
    {"stimeout", "5000000"},               // 网络连接和读超时（微秒）
    {"fflags", "nobuffer+genpts+igndts"},  // 禁用内部缓冲，减少延迟
    {"flush_packets", "1"},                // 每包立即处理，减少延迟
    {"analyzeduration", "0"},              // 禁止流分析，快速启动
    {"framedrop", "1"},                    // 丢帧防止阻塞，实时性关键
    {"reconnect", "1"},                    // 断线自动重连
    {"reconnect_at_eof", "1"},             // 流结束自动重连
    {"reconnect_streamed", "1"},           // 指定网络流
    {"reconnect_delay_max", "5"},          // 最大重连间隔秒
    {"avioflags", "8"},                    // 非阻塞IO
    {"flags", "low_delay"},                // 编解码低延迟标志
    {"ignore_unknown", "1"},
    {"probesize", "5000000"},
};

inline std::map<const char*, const char*> rtspDictMap{
    {"buffer_size", "32768"},              // 网络缓冲大小，三协议均有效，UDP通常可适当调小
    {"stimeout", "5000000"},               // 网络连接和读超时（微秒）
    {"fflags", "nobuffer+genpts+igndts"},  // 禁用内部缓冲，减少延迟
    {"flush_packets", "1"},                // 每包立即处理，减少延迟
    {"analyzeduration", "0"},              // 禁止流分析，快速启动
    {"framedrop", "1"},                    // 丢帧防止阻塞，实时性关键
    {"reconnect", "1"},                    // 断线自动重连
    {"reconnect_at_eof", "1"},             // 流结束自动重连
    {"reconnect_streamed", "1"},           // 指定网络流
    {"reconnect_delay_max", "5"},          // 最大重连间隔秒
    {"avioflags", "8"},                    // 非阻塞IO
    {"flags", "low_delay"},                // 编解码低延迟标志
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
