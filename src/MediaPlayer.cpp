#include "MediaPlayer.h"

#include <QPushButton>
#include <QResizeEvent>
#include <thread>

MediaPlayer::MediaPlayer(QWidget* _parent) : QWidget{_parent}
{
    std::invoke(&MediaPlayer::initMediaPlayer, this);
    std::invoke(&MediaPlayer::connectSignalToSlot, this);
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

    av->setStreamUrl(R"(rtmp://liteavapp.qcloud.com/live/liteavdemoplayerstreamid)");
    av->mediumStart();
    //----
    // QPushButton* btn{new QPushButton{"ssss", this}};
    // btn->setGeometry(50, 50, 200, 40);
    // btn->setStyleSheet(R"(
    //     QPushButton {
    //         background-color: rgba(0, 0, 0, 0);  /* 完全透明背景 */
    //         color: rgba(0, 0, 0, 0);             /* 文字透明 */
    //         border: none;                       /* 无边框 */
    //     }
    //     QPushButton:hover {
    //         background-color: rgba(0, 0, 0, 0);  /* 仍然透明背景 */
    //         color: white;                       /* 显示白色文字 */
    //         border: 1px solid #2980b9;          /* 显示边框 */
    //     }
    //     QPushButton:pressed {
    //         background-color: rgba(200, 200, 200, 80); /* 点击时浅灰背景，80为透明度 */
    //     }
    // )");

    std::thread{[this]() {
        auto gen = av->flushPacket();

        SwsContext* swsCtx  = nullptr;
        uint8_t*    buffer  = nullptr;
        int         bufSize = 0;

        while (gen.next())
        {
            AVFrame*      frame     = gen.current();
            int           width     = frame->width;
            int           height    = frame->height;
            AVPixelFormat srcFormat = static_cast<AVPixelFormat>(frame->format);

            // 如果swsCtx还没创建，或者分辨率改变，重新创建swsCtx和buffer
            if (!swsCtx || bufSize != width * height * 4)
            {
                if (swsCtx)
                    sws_freeContext(swsCtx);
                if (buffer)
                    av_free(buffer);

                swsCtx = sws_getContext(
                    width, height, srcFormat,
                    width, height, AV_PIX_FMT_RGBA,
                    SWS_BILINEAR,
                    nullptr, nullptr, nullptr);

                bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
                buffer  = (uint8_t*)av_malloc(bufSize);
            }

            uint8_t* dstData[4]     = {buffer, nullptr, nullptr, nullptr};
            int      dstLinesize[4] = {4 * width, 0, 0, 0};

            sws_scale(swsCtx,
                      frame->data, frame->linesize,
                      0, height,
                      dstData, dstLinesize);

            // 直接用buffer构造QImage，不拷贝内存，避免性能损失
            QImage  image(buffer, width, height, dstLinesize[0], QImage::Format_RGBA8888);
            QPixmap pixmap = QPixmap::fromImage(image.copy());  // 这里copy是为了安全，避免线程问题
            if (pixmap.isNull())
            {
                continue;
            }
            this->setFramePix(pixmap);
        }
        if (swsCtx)
            sws_freeContext(swsCtx);
        if (buffer)
            av_free(buffer);
    }}.detach();

    m_graphicsView->show();
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
    QPixmap scaled = m_framePix.scaled(this->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_graphicsPixmapItem->setPixmap(scaled);

    QWidget::resizeEvent(_event);
}

void MediaPlayer::onFramePixChanged(QPixmap _pixmap)
{
    m_framePix     = _pixmap;
    QPixmap scaled = m_framePix.scaled(this->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_graphicsPixmapItem->setPixmap(scaled);
}

QPixmap avframeToQPixmap(AVFrame* frame, int width, int height, SwsContext* swsCtx)
{
    // 分配 RGB/RGBA Frame
    AVFrame* rgbFrame = av_frame_alloc();
    int      numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 1);
    uint8_t* buffer   = (uint8_t*)av_malloc(numBytes);
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer,
                         AV_PIX_FMT_RGBA, width, height, 1);

    // 执行转换
    sws_scale(swsCtx, frame->data, frame->linesize, 0, height,
              rgbFrame->data, rgbFrame->linesize);

    // 用转换后的数据构建 QImage（不拷贝内存）
    QImage image(rgbFrame->data[0], width, height, rgbFrame->linesize[0],
                 QImage::Format_RGBA8888);

    // 为避免 buffer 被释放，执行拷贝构造
    QPixmap pixmap = QPixmap::fromImage(image.copy());

    // 清理内存
    av_free(buffer);
    av_frame_free(&rgbFrame);

    return pixmap;
}
