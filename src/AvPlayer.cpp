#include "AvPlayer.h"

#include <QPushButton>
#include <QResizeEvent>
#include <thread>

MediaPlay::MediaPlay(QWidget* _parent) : QWidget{_parent}
{
    std::invoke(&MediaPlay::initGraphics, this);
}

auto MediaPlay::initGraphics() noexcept -> void
{
    m_graphicsScene->setSceneRect(this->rect());
    m_graphicsScene->setBackgroundBrush(Qt::black);
    m_graphicsView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_graphicsView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_graphicsView->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->addWidget(m_graphicsView);
    m_graphicsPixmapItem->setPos(0, 0);
    m_originalPixmap = QPixmap{R"(C:\Users\ZZY99\Desktop\AVSources\destoke.png)"};
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

    // 初次缩放
    m_graphicsScene->addItem(m_graphicsPixmapItem);
    QPixmap scaled = m_originalPixmap.scaled(this->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_graphicsPixmapItem->setPixmap(scaled);
    connect(this, &MediaPlay::pixmapChanged, this, &MediaPlay::onPixmapChanged);
    av->setStreamUrl(R"(rtmp://liteavapp.qcloud.com/live/liteavdemoplayerstreamid)");
    av->mediumStart();

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

            emit pixmapChanged(pixmap);
        }

        if (swsCtx)
            sws_freeContext(swsCtx);
        if (buffer)
            av_free(buffer);
    }}.detach();

    m_graphicsView->show();
}

void MediaPlay::resizeEvent(QResizeEvent* event)
{
    m_graphicsScene->setSceneRect(this->rect());
    // m_graphicsVideoItem->setPos(0, 0);
    // m_graphicsVideoItem->setSize(this->size());
    QPixmap scaled = m_originalPixmap.scaled(this->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_graphicsPixmapItem->setPixmap(scaled);

    QWidget::resizeEvent(event);
}

void MediaPlay::onPixmapChanged(QPixmap _pixmap)
{
    m_originalPixmap = _pixmap;
    QPixmap scaled   = m_originalPixmap.scaled(this->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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
