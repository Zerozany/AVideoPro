#include "MediaView.h"

#include <QWheelEvent>

MediaView::MediaView(QGraphicsScene* _parent) : QGraphicsView{_parent}
{
}

void MediaView::wheelEvent(QWheelEvent* _event)
{
    return;
}
