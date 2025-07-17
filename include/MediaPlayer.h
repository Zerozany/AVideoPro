_Pragma("once");
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QVBoxLayout>
#include <QWidget>
extern "C" {
#include <libavutil/imgutils.h>
}

#include "MediaView.h"
#include "MediumFrame.h"

class QResizeEvent;

class MediaPlayer : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QPixmap framePix READ getFramePix WRITE setFramePix NOTIFY framePixChanged)
public:
    explicit(true) MediaPlayer(QWidget* _parent = nullptr);
    ~MediaPlayer() noexcept = default;

public:
    auto getFramePix() const noexcept -> QPixmap;
    auto setFramePix(const QPixmap& _pixmap) noexcept -> void;

private:
    auto initMediaPlayer() noexcept -> void;

    auto connectSignalToSlot() noexcept -> void;

protected:
    void resizeEvent(QResizeEvent* _event) override;

Q_SIGNALS:
    void framePixChanged(QPixmap _pixmap);

private Q_SLOTS:
    void onFramePixChanged(QPixmap _pixmap);

private:
    QVBoxLayout*         m_mainLayout{new QVBoxLayout{this}};
    QGraphicsScene*      m_graphicsScene{new QGraphicsScene{}};
    MediaView*           m_graphicsView{new MediaView{m_graphicsScene}};
    QGraphicsPixmapItem* m_graphicsPixmapItem{new QGraphicsPixmapItem{}};
    QPixmap              m_framePix{};
    MediumFrame*         av{new MediumFrame{}};
};

QPixmap avframeToQPixmap(AVFrame* frame, int width, int height, SwsContext* swsCtx);
