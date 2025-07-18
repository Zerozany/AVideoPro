#include "MediaPlayer.h"

#include <spdlog/spdlog.h>

#include <QPushButton>
#include <QResizeEvent>
#include <thread>

MediaPlayer::MediaPlayer(QWidget* _parent) : QWidget{_parent}
{
    std::invoke(&MediaPlayer::initMediaPlayer, this);
    std::invoke(&MediaPlayer::connectSignalToSlot, this);
}

auto MediaPlayer::play() noexcept -> void
{
    if (!m_mediumFrame->getFrameState())
    {
        spdlog::error("Live streaming address resolution failed or is invalid");
        return;
    }
    std::thread{[this] {
        auto        gen{m_mediumFrame->flushPacket()};
        SwsContext* swsCtx{nullptr};
        uint8_t*    buffer{nullptr};
        AVFrame*    frame{nullptr};
        int         bufSize{};
        while (gen.next() && m_mediumFrame->getFrameState())
        {
            frame = gen.current();
            int           width{frame->width};
            int           height{frame->height};
            AVPixelFormat srcFormat{static_cast<AVPixelFormat>(frame->format)};
            // 如果swsCtx还没创建，或者分辨率改变，重新创建swsCtx和buffer
            if (!swsCtx || bufSize != width * height * 4)
            {
                if (swsCtx)
                {
                    sws_freeContext(swsCtx);
                }
                if (buffer)
                {
                    av_free(buffer);
                }
                swsCtx = sws_getContext(
                    width, height, srcFormat,
                    width, height, AV_PIX_FMT_RGBA,
                    SWS_BILINEAR,
                    nullptr, nullptr, nullptr);
                bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
                buffer  = (uint8_t*)av_malloc(bufSize);
            }
            uint8_t* dstData[4]{buffer, nullptr, nullptr, nullptr};
            int      dstLinesize[4]{4 * width, 0, 0, 0};
            sws_scale(swsCtx, frame->data, frame->linesize, 0, height, dstData, dstLinesize);
            // 直接用buffer构造QImage，不拷贝内存，避免性能损失
            QImage  image{buffer, width, height, dstLinesize[0], QImage::Format_RGBA8888};
            QPixmap pixmap{QPixmap::fromImage(image.copy())};
            if (pixmap.isNull())
            {
                continue;
            }
            this->setFramePix(std::move(pixmap));
            if (frame)
            {
                av_frame_unref(frame);
            }
        }
        if (swsCtx)
        {
            sws_freeContext(swsCtx);
        }
        if (buffer)
        {
            av_free(buffer);
        }
        if (frame)
        {
            av_frame_free(&frame);
        }
    }}.detach();
}

auto MediaPlayer::setUrl(const std::string& _url) noexcept -> void
{
    m_mediumFrame->setUrl(_url);
    m_mediumFrame->mediumStart();
}

auto MediaPlayer::getFramePix() const noexcept -> QPixmap
{
    return this->m_framePix;
}

auto MediaPlayer::setFramePix(const QPixmap& _pixmap) noexcept -> void
{
    m_framePix = _pixmap;
    Q_EMIT this->framePixChanged(m_framePix);
}

auto MediaPlayer::initMediaPlayer() noexcept -> void
{
    m_graphicsScene->setSceneRect(this->rect());
    m_graphicsScene->setBackgroundBrush(Qt::black);
    m_graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_graphicsView->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->addWidget(m_graphicsView);
    m_graphicsScene->addItem(m_graphicsPixmapItem);
    m_graphicsPixmapItem->setPos(0, 0);
    m_graphicsView->show();

    //----
    QPushButton* btn{new QPushButton{"ssss", this}};
    btn->setGeometry(50, 50, 200, 40);
    btn->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(0, 0, 0, 0);  /* 完全透明背景 */
            color: rgba(0, 0, 0, 0);             /* 文字透明 */
            border: none;                       /* 无边框 */
        }
        QPushButton:hover {
            background-color: rgba(0, 0, 0, 0);  /* 仍然透明背景 */
            color: white;                       /* 显示白色文字 */
            border: 1px solid #2980b9;          /* 显示边框 */
        }
        QPushButton:pressed {
            background-color: rgba(200, 200, 200, 80); /* 点击时浅灰背景，80为透明度 */
        }
    )");
    connect(btn, &QPushButton::clicked, this, [this] {
        setUrl(std::string{R"(http://devimages.apple.com.edgekey.net/streaming/examples/bipbop_4x3/gear2/prog_index.m3u8)"});
    });

    QPushButton* btn1{new QPushButton{"xxxx", this}};
    btn1->setGeometry(50, 100, 200, 40);
    connect(btn1, &QPushButton::clicked, this, [this] {
        this->play();
    });
}

auto MediaPlayer::connectSignalToSlot() noexcept -> void
{
    connect(this, &MediaPlayer::framePixChanged, this, &MediaPlayer::onFramePixChanged);
}

void MediaPlayer::resizeEvent(QResizeEvent* _event)
{
    m_graphicsScene->setSceneRect(this->rect());
    if (m_framePix.isNull())
    {
        return;
    }
    QPixmap scaled{m_framePix.scaled(this->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)};
    m_graphicsPixmapItem->setPixmap(scaled);

    QWidget::resizeEvent(_event);
}

void MediaPlayer::onFramePixChanged(QPixmap _pixmap)
{
    m_framePix = _pixmap;
    QPixmap scaled{m_framePix.scaled(this->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)};
    m_graphicsPixmapItem->setPixmap(scaled);
}
