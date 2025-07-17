_Pragma("once");
#include <QObject>
#include <coroutine>
#include <exception>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

struct MediumFrameGenerator
{
    struct promise_type
    {
        auto get_return_object() -> MediumFrameGenerator
        {
            return MediumFrameGenerator{std::coroutine_handle<promise_type>::from_promise(*this)};
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

    explicit(true) MediumFrameGenerator(std::coroutine_handle<promise_type> _handle) : m_handle(_handle) {}
    ~MediumFrameGenerator()
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

class MediumFrame : public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::string url READ getUrl WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(UrlFormat urlFormat READ getUrlFormat WRITE setUrlFormat NOTIFY urlFormatChanged)
public:
    enum struct UrlFormat
    {
        OTHER = 0x00,
        RTSP  = 0x01,
        RTMP  = 0x02,
        UDP   = 0x03,
    };
    Q_ENUM(UrlFormat)

public:
    explicit(true) MediumFrame(QObject* _parent = nullptr);
    explicit(true) MediumFrame(const std::string& _url, QObject* _parent = nullptr);
    ~MediumFrame() noexcept;

public:
    auto getUrl() const noexcept -> std::string;
    auto setUrl(const std::string& _url) noexcept -> void;

    auto getUrlFormat() const noexcept -> UrlFormat;
    auto setUrlFormat(const UrlFormat& _urlFormat) noexcept -> void;

public:
    auto flushPacket() noexcept -> MediumFrameGenerator;

    auto mediumStart() noexcept -> void;

    auto getFrameState() noexcept -> bool;

private:
    auto connectSignalToSlot() noexcept -> void;

private:
    auto avOpenInput() noexcept -> bool;

    auto findVideoStream() noexcept -> bool;

    auto initCodecContext() noexcept -> bool;

Q_SIGNALS:
    void urlChanged();

    void urlFormatChanged();

private Q_SLOTS:
    void onUrlChanged();

    void onUrlFormatChanged();

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
    bool             m_frameHandle{false};
};
