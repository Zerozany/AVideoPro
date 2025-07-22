#include "MediaPlay.h"

#include <QResizeEvent>

MediaPlay::MediaPlay(QWidget* _parent) : QWidget{_parent}
{
    std::invoke(&MediaPlay::initPlayerLayout, this);
    std::invoke(&MediaPlay::connectSignalToSlot, this);
}

MediaPlay::~MediaPlay() noexcept
{
    if (m_mediaFrame)
    {
        m_mediaFrame.get()->stop();
    }
}

auto MediaPlay::setUrl(const std::string& _url) noexcept -> void
{
    if (m_mediaFrame)
    {
        m_mediaFrame.get()->stop();
    }
    m_mediaFrame.reset(new MediaFrame{});
    m_mediaFrame.get()->setStreamUrl(_url);
    if (!m_mediaFrame.get()->start())
    {
        return;
    }
}

auto MediaPlay::play() noexcept -> void
{
    auto gen{m_mediaFrame->flushPacket()};
    while (gen.nextValue())
    {
        auto   data = gen.current();
        QImage img{data.rgbBuffer.data(), data.width, data.height, data.lineSize, QImage::Format_RGB888};
        this->setFramePix(QPixmap::fromImage(img));
    }
}

auto MediaPlay::stop() noexcept -> void
{
    if (m_mediaFrame)
    {
        m_mediaFrame.get()->stop();
    }
}

auto MediaPlay::getFramePix() const noexcept -> QPixmap
{
    return this->m_framePix;
}

auto MediaPlay::setFramePix(const QPixmap& _pixmap) noexcept -> void
{
    if (m_framePix.cacheKey() == _pixmap.cacheKey())
    {
        return;
    }
    m_framePix = _pixmap;
    Q_EMIT this->framePixChanged();
}

auto MediaPlay::initPlayerLayout() noexcept -> void
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
}

auto MediaPlay::connectSignalToSlot() noexcept -> void
{
    connect(this, &MediaPlay::framePixChanged, this, &MediaPlay::onFramePixChanged);
}

auto MediaPlay::getMediaFrame() noexcept -> void
{
}

void MediaPlay::resizeEvent(QResizeEvent* _event)
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

void MediaPlay::onFramePixChanged()
{
    QPixmap scaled{this->getFramePix().scaled(this->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)};
    m_graphicsPixmapItem->setPixmap(scaled);
}
