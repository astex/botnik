#include "compositor.h"

Compositor::Compositor(QQuickWindow *window)
    : QWaylandCompositor()
{
    m_output = new QWaylandOutput(this, window);

    QWaylandOutputMode mode(window->size(), 60000);
    m_output->addMode(mode, true);
    m_output->setCurrentMode(mode);

    create();
}
