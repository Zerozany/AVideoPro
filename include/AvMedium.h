_Pragma("once");
#include <coroutine>
#include <exception>
#include <iostream>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

struct AvMediumGenerator
{
    struct promise_type
    {
        auto get_return_object() -> AvMediumGenerator
        {
            return AvMediumGenerator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        auto initial_suspend() noexcept -> std::suspend_always { return {}; }
        auto final_suspend() noexcept -> std::suspend_always { return {}; }
        auto yield_value(AVFrame* frame) noexcept -> std::suspend_always
        {
            m_frame = frame;
            return {};
        }
        auto unhandled_exception() -> void { std::terminate(); }
        auto return_void() -> void {}

        AVFrame* m_frame{nullptr};
    };

    std::coroutine_handle<promise_type> m_handle;

    explicit(true) AvMediumGenerator(std::coroutine_handle<promise_type> _handle) : m_handle(_handle) {}
    ~AvMediumGenerator()
    {
        if (m_handle)
        {
            m_handle.destroy();
        }
    }

    auto next() -> bool
    {
        if (!m_handle || m_handle.done())
        {
            return false;
        }
        m_handle.resume();
        return !m_handle.done();
    }

    auto current() -> AVFrame*
    {
        return m_handle.promise().m_frame;
    }
};

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
    explicit(true) AvMedium();
    ~AvMedium() noexcept;

public:
    auto flushPacket() noexcept -> AvMediumGenerator;

    auto setStreamUrl(const std::string& _url) noexcept -> void;

    auto mediumStart() noexcept -> void;

private:
    auto smuSetOptions() noexcept -> void;

    auto rtspSetOptions() noexcept -> void;

    auto rtmpSetOptions() noexcept -> void;

    auto udpSetOptions() noexcept -> void;

    auto avOpenInput() noexcept -> bool;

    auto findVideoStream() noexcept -> bool;

    auto initCodecContext() noexcept -> bool;

private:
    std::string      m_url{};
    UrlFormat        m_urlFormat{};
    AVDictionary*    m_options{nullptr};
    AVFormatContext* m_formatCtx{avformat_alloc_context()};
    int              m_videoIndex{};
    AVCodecContext*  m_codecCtx{nullptr};
    SwsContext*      m_swsCtx{nullptr};
    AVPacket*        m_packet{av_packet_alloc()};
    AVFrame*         m_frame{av_frame_alloc()};
};
