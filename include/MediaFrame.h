_Pragma("once");
#include <QObject>
#include <atomic>
#include <map>
#include <string>

#include "Generator.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class MediaFrame : public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::string url READ getUrl WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(UrlType urlType READ getUrlType WRITE setUrlType NOTIFY urlTypeChanged)
public:
    enum struct UrlType
    {
        OTHER = 0x00,
        RTSP  = 0x01,
        RTMP  = 0x02,
        UDP   = 0x03,
    };
    Q_ENUM(UrlType);

public:
    explicit(true) MediaFrame();
    ~MediaFrame() noexcept;

public:
    auto getUrl() const noexcept -> std::string;
    auto setUrl(const std::string& _url) noexcept -> void;

    auto getUrlType() const noexcept -> UrlType;
    auto setUrlType(const UrlType& _urlType) noexcept -> void;

public:
    auto flushPacket() noexcept -> Generator<AVFrame>;

    auto getMediaState() noexcept -> bool;

private:
    auto connectSignalToSlot() noexcept -> void;

    auto mediaOpenInput() noexcept -> bool;

    auto findVideoStream() noexcept -> bool;

    auto codecContext() noexcept -> bool;

Q_SIGNALS:
    void urlChanged();

    void urlTypeChanged();

private Q_SLOTS:
    auto onUrlChanged() noexcept -> void;

    auto onUrlTypeChanged() noexcept -> void;

private:
    std::string       m_url{};
    UrlType           m_urlType{};
    AVDictionary*     m_options{nullptr};
    AVFormatContext*  m_formatCtx{nullptr};
    int               m_videoIndex{};
    AVCodecContext*   m_codecCtx{nullptr};
    SwsContext*       m_swsCtx{nullptr};
    AVPacket*         m_packet{nullptr};
    AVFrame*          m_frame{nullptr};
    std::atomic<bool> m_frameHandle{false};

private:
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
        {"rtmp_tcp_nodelay", "1"},     // RTMP专用，禁用Nagle算法，减少延迟
        {"rtmp_buffer", "32768"},      // RTMP专用缓冲大小
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

    std::map<const char*, const char*> udpOptionsMap{};
};
