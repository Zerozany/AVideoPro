_Pragma("once");
#include <QGraphicsView>
#include <QWidget>

class QWheelEvent;

class MediaView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit(true) MediaView(QGraphicsScene* _parent = nullptr);
    ~MediaView() noexcept = default;

protected:
    void wheelEvent(QWheelEvent* _event) override;
};
