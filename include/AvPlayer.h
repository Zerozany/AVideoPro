_Pragma("once");
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QVBoxLayout>
#include <QWidget>
extern "C" {
#include <libavutil/imgutils.h>
}

#include "AvMedium.h"
#include "GraphicsView.h"

class QResizeEvent;

class MediaPlay : public QWidget
{
    Q_OBJECT
public:
    explicit(true) MediaPlay(QWidget* _parent = nullptr);
    ~MediaPlay() noexcept = default;

private:
    auto initGraphics() noexcept -> void;

protected:
    void resizeEvent(QResizeEvent* _event) override;

Q_SIGNALS:
    void pixmapChanged(QPixmap _pixmap);

private Q_SLOTS:
    void onPixmapChanged(QPixmap _pixmap);

private:
    QVBoxLayout*         m_mainLayout{new QVBoxLayout{this}};
    QGraphicsScene*      m_graphicsScene{new QGraphicsScene{0, 0, 0, 0}};
    GraphicsView*        m_graphicsView{new GraphicsView{m_graphicsScene}};
    QGraphicsPixmapItem* m_graphicsPixmapItem{new QGraphicsPixmapItem{}};
    QPixmap              m_originalPixmap{};
    AvMedium*            av{new AvMedium{}};
};

QPixmap avframeToQPixmap(AVFrame* frame, int width, int height, SwsContext* swsCtx);
