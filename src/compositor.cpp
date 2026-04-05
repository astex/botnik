#include "compositor.h"
#include <QWaylandSurface>

// --- SurfaceModel ---

SurfaceModel::SurfaceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SurfaceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_surfaces.size();
}

QVariant SurfaceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_surfaces.size())
        return {};

    if (role == XdgSurfaceRole)
        return QVariant::fromValue(m_surfaces.at(index.row()));

    return {};
}

QHash<int, QByteArray> SurfaceModel::roleNames() const
{
    return {{XdgSurfaceRole, "xdgSurface"}};
}

void SurfaceModel::addSurface(QWaylandXdgSurface *surface)
{
    beginInsertRows(QModelIndex(), m_surfaces.size(), m_surfaces.size());
    m_surfaces.append(surface);
    endInsertRows();
}

void SurfaceModel::removeSurface(QWaylandXdgSurface *surface)
{
    int idx = m_surfaces.indexOf(surface);
    if (idx < 0)
        return;
    beginRemoveRows(QModelIndex(), idx, idx);
    m_surfaces.removeAt(idx);
    endRemoveRows();
}

// --- Compositor ---

Compositor::Compositor(QQuickWindow *window)
    : QWaylandCompositor()
{
    m_output = new QWaylandOutput(this, window);

    QWaylandOutputMode mode(window->size(), 60000);
    m_output->addMode(mode, true);
    m_output->setCurrentMode(mode);

    m_xdgShell = new QWaylandXdgShell(this);
    connect(m_xdgShell, &QWaylandXdgShell::toplevelCreated,
            this, &Compositor::onToplevelCreated);

    create();
}

void Compositor::onToplevelCreated(QWaylandXdgToplevel *toplevel,
                                   QWaylandXdgSurface *xdgSurface)
{
    // Send initial configure so the client knows the compositor accepted it.
    toplevel->sendConfigure({300, 40}, QList<QWaylandXdgToplevel::State>());

    m_surfaceModel.addSurface(xdgSurface);

    // Clean up when the surface is destroyed.
    connect(xdgSurface->surface(), &QWaylandSurface::surfaceDestroyed,
            this, [this, xdgSurface]() {
                m_surfaceModel.removeSurface(xdgSurface);
            });
}
