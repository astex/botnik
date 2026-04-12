#include "compositor.h"
#include <QWaylandSurface>

// --- WorkspaceModel ---

WorkspaceModel::WorkspaceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int WorkspaceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_workspaces.size();
}

QVariant WorkspaceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_workspaces.size())
        return {};

    if (role == XdgSurfaceRole)
        return QVariant::fromValue(m_workspaces.at(index.row()).surface);

    return {};
}

QHash<int, QByteArray> WorkspaceModel::roleNames() const
{
    return {{XdgSurfaceRole, "xdgSurface"}};
}

QWaylandXdgToplevel *WorkspaceModel::toplevelAt(int row) const
{
    if (row < 0 || row >= m_workspaces.size())
        return nullptr;
    return m_workspaces.at(row).toplevel;
}

void WorkspaceModel::addWorkspace(QWaylandXdgSurface *surface,
                                  QWaylandXdgToplevel *toplevel)
{
    beginInsertRows(QModelIndex(), m_workspaces.size(), m_workspaces.size());
    m_workspaces.append({surface, toplevel});
    endInsertRows();

    // Newest wins.
    setActiveIndex(m_workspaces.size() - 1);
}

void WorkspaceModel::removeBySurface(QWaylandXdgSurface *surface)
{
    int idx = -1;
    for (int i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces.at(i).surface == surface) {
            idx = i;
            break;
        }
    }
    if (idx < 0)
        return;

    beginRemoveRows(QModelIndex(), idx, idx);
    m_workspaces.removeAt(idx);
    endRemoveRows();

    // Fix up activeIndex:
    //  - if the removed row was active, fall back to the new last row (next-newest alive);
    //    if the list is empty, -1.
    //  - if the removed row was before the active one, decrement to keep pointing at
    //    the same workspace.
    //  - otherwise leave activeIndex alone.
    int newActive = m_activeIndex;
    if (m_workspaces.isEmpty()) {
        newActive = -1;
    } else if (idx == m_activeIndex) {
        newActive = m_workspaces.size() - 1;
    } else if (idx < m_activeIndex) {
        newActive = m_activeIndex - 1;
    }
    setActiveIndex(newActive);
}

void WorkspaceModel::setActiveIndex(int index)
{
    if (index == m_activeIndex)
        return;
    m_activeIndex = index;
    emit activeIndexChanged();
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

void Compositor::setClientArea(int width, int height)
{
    QSize newSize(width, height);
    if (newSize == m_clientArea)
        return;
    m_clientArea = newSize;

    // Re-send configure to every live toplevel so clients resize to match.
    for (int i = 0; i < m_workspaceModel.count(); ++i) {
        if (auto *tl = m_workspaceModel.toplevelAt(i))
            tl->sendConfigure(m_clientArea, QList<QWaylandXdgToplevel::State>());
    }
}

void Compositor::onToplevelCreated(QWaylandXdgToplevel *toplevel,
                                   QWaylandXdgSurface *xdgSurface)
{
    // Initial configure uses the current client-area size. If QML hasn't
    // reported one yet (m_clientArea is still {0,0}) the client will just
    // pick its own size; the next setClientArea() call will correct it.
    toplevel->sendConfigure(m_clientArea, QList<QWaylandXdgToplevel::State>());

    m_workspaceModel.addWorkspace(xdgSurface, toplevel);

    // Clean up when the surface is destroyed.
    connect(xdgSurface->surface(), &QWaylandSurface::surfaceDestroyed,
            this, [this, xdgSurface]() {
                m_workspaceModel.removeBySurface(xdgSurface);
            });
}
