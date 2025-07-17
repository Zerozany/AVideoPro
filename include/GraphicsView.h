_Pragma("once");
#include <QGraphicsView>
#include <QWidget>

class GraphicsView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit(true) GraphicsView(QGraphicsScene* _parent = nullptr);
    ~GraphicsView() noexcept = default;
};
