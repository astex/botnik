#pragma once

#include <QWaylandCompositor>
#include <QWaylandOutput>
#include <QQuickWindow>

class Compositor : public QWaylandCompositor {
    Q_OBJECT

public:
    explicit Compositor(QQuickWindow *window);

private:
    QWaylandOutput *m_output = nullptr;
};
