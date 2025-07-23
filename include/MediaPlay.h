_Pragma("once");
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>

#include "MediaFrame.h"
#include "MediaView.h"

class MediaPlay : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QPixmap framePix READ getFramePix WRITE setFramePix NOTIFY framePixChanged)

public:
    explicit(true) MediaPlay(QWidget* _parent = nullptr);
    ~MediaPlay() noexcept;

public:
    auto setUrl(const std::string& _url) noexcept -> void;

    auto play() noexcept -> void;

    auto stop() noexcept -> void;

private:
    auto getFramePix() const noexcept -> QPixmap;
    auto setFramePix(const QPixmap& _pixmap) noexcept -> void;

private:
    auto initPlayerLayout() noexcept -> void;

    auto connectSignalToSlot() noexcept -> void;

    auto getMediaFrame() noexcept -> void;

protected:
    void resizeEvent(QResizeEvent* _event) override;

Q_SIGNALS:
    void framePixChanged();

private Q_SLOTS:
    void onFramePixChanged();

private:
    std::unique_ptr<MediaFrame> m_mediaFrame{nullptr};
    QVBoxLayout*                m_mainLayout{new QVBoxLayout{this}};
    QGraphicsScene*             m_graphicsScene{new QGraphicsScene{}};
    MediaView*                  m_graphicsView{new MediaView{m_graphicsScene}};
    QGraphicsPixmapItem*        m_graphicsPixmapItem{new QGraphicsPixmapItem{}};
    QPixmap                     m_framePix{};

    QPushButton* btn1{new QPushButton{"play", this}};
    QPushButton* btn2{new QPushButton{"stop", this}};
    QLineEdit*   edit{new QLineEdit{this}};
};
