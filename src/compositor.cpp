#include "compositor.h"
#include <QWaylandSeat>
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

    const auto &ws = m_workspaces.at(index.row());

    switch (role) {
    case XdgSurfaceRole:
        return QVariant::fromValue(ws.surface);
    case SurfaceIdRole:
        return ws.id;
    case TitleRole: {
        QString title = ws.toplevel ? ws.toplevel->title() : QString();
        if (title.isEmpty())
            title = QStringLiteral("Window %1").arg(index.row() + 1);
        return title;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> WorkspaceModel::roleNames() const
{
    return {
        {XdgSurfaceRole, "xdgSurface"},
        {SurfaceIdRole, "surfaceId"},
        {TitleRole, "title"},
    };
}

QWaylandXdgToplevel *WorkspaceModel::toplevelAt(int row) const
{
    if (row < 0 || row >= m_workspaces.size())
        return nullptr;
    return m_workspaces.at(row).toplevel;
}

int WorkspaceModel::findById(int id) const
{
    for (int i = 0; i < m_workspaces.size(); ++i) {
        if (m_workspaces.at(i).id == id)
            return i;
    }
    return -1;
}

void WorkspaceModel::addWorkspace(QWaylandXdgSurface *surface,
                                  QWaylandXdgToplevel *toplevel)
{
    beginInsertRows(QModelIndex(), m_workspaces.size(), m_workspaces.size());
    m_workspaces.append({m_nextId++, surface, toplevel});
    endInsertRows();

    // Update title when the client sets it.
    if (toplevel) {
        int row = m_workspaces.size() - 1;
        connect(toplevel, &QWaylandXdgToplevel::titleChanged, this,
                [this, row]() {
                    if (row < m_workspaces.size()) {
                        QModelIndex idx = index(row);
                        emit dataChanged(idx, idx, {TitleRole});
                    }
                });
    }

    // Newest wins.
    setActiveIndex(m_workspaces.size() - 1);
    emit countChanged();
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
    emit countChanged();
}

void WorkspaceModel::activateWorkspace(int index)
{
    if (index >= 0 && index < m_workspaces.size())
        setActiveIndex(index);
}

void WorkspaceModel::setActiveIndex(int index)
{
    if (index == m_activeIndex)
        return;
    m_activeIndex = index;
    emit activeIndexChanged();
}

// --- CompositorKeyboard ---

CompositorKeyboard::CompositorKeyboard(Compositor *compositor, QWaylandSeat *seat)
    : QWaylandKeyboard(seat)
    , m_compositor(compositor)
{
    // Defer scan-code lookup until after the keyboard is fully initialized
    // (XKB keymap must be loaded first).
    QMetaObject::invokeMethod(this, &CompositorKeyboard::buildScanCodeMap,
                              Qt::QueuedConnection);
}

void CompositorKeyboard::buildScanCodeMap()
{
    m_metaLeftCode = keyToScanCode(Qt::Key_Super_L);
    m_metaRightCode = keyToScanCode(Qt::Key_Super_R);

    // Meta+Space -> focus chat
    uint spaceCode = keyToScanCode(Qt::Key_Space);
    if (spaceCode)
        m_hotkeyMap.insert(spaceCode, HotkeyFocusChat);

    // Meta+1..9 -> workspace switching
    for (int n = 1; n <= 9; ++n) {
        uint code = keyToScanCode(Qt::Key_0 + n);
        if (code)
            m_hotkeyMap.insert(code, HotkeyWorkspace1 + n - 1);
    }
}

void CompositorKeyboard::sendKeyPressEvent(uint code)
{
    // Track Meta modifier.
    if (code == m_metaLeftCode || code == m_metaRightCode) {
        m_metaHeld = true;
        QWaylandKeyboard::sendKeyPressEvent(code);
        return;
    }

    if (m_metaHeld) {
        auto it = m_hotkeyMap.find(code);
        if (it != m_hotkeyMap.end()) {
            int id = it.value();
            if (id == HotkeyFocusChat) {
                emit m_compositor->hotkeyFocusChat();
            } else {
                // HotkeyWorkspace1..9 -> index 0..8
                int wsIndex = id - HotkeyWorkspace1;
                emit m_compositor->hotkeyActivateWorkspace(wsIndex);
            }
            return; // Consume — don't forward to client.
        }
    }

    QWaylandKeyboard::sendKeyPressEvent(code);
}

void CompositorKeyboard::sendKeyReleaseEvent(uint code)
{
    if (code == m_metaLeftCode || code == m_metaRightCode) {
        m_metaHeld = false;
    }

    QWaylandKeyboard::sendKeyReleaseEvent(code);
}

// --- Compositor ---

Compositor::Compositor(QQuickWindow *window)
    : QWaylandCompositor()
    , m_window(window)
{
    m_output = new QWaylandOutput(this, window);

    QWaylandOutputMode mode(window->size(), 60000);
    m_output->addMode(mode, true);
    m_output->setCurrentMode(mode);

    m_xdgShell = new QWaylandXdgShell(this);
    connect(m_xdgShell, &QWaylandXdgShell::toplevelCreated,
            this, &Compositor::onToplevelCreated);

    // Re-tile when the number of windows changes.
    connect(&m_workspaceModel, &WorkspaceModel::countChanged,
            this, &Compositor::reconfigureTiles);

    create();
}

QWaylandKeyboard *Compositor::createKeyboardDevice(QWaylandSeat *seat)
{
    return new CompositorKeyboard(this, seat);
}

void Compositor::setClientArea(int width, int height)
{
    QSize newSize(width, height);
    if (newSize == m_clientArea)
        return;
    m_clientArea = newSize;
    reconfigureTiles();
}

void Compositor::reconfigureTiles()
{
    const int n = m_workspaceModel.count();
    if (n == 0 || m_clientArea.isEmpty())
        return;

    const int cols = qMin(n, 2);
    const int rows = (n + cols - 1) / cols;

    for (int i = 0; i < n; ++i) {
        auto *tl = m_workspaceModel.toplevelAt(i);
        if (!tl)
            continue;

        const int row = i / cols;
        const int col = i % cols;
        // Items in the last row may span wider if the row is not full.
        const int itemsInRow = (row == rows - 1) ? (n - row * cols) : cols;
        const int tileW = m_clientArea.width() / itemsInRow;
        const int tileH = m_clientArea.height() / rows;

        Q_UNUSED(col);
        tl->sendConfigure(QSize(tileW, tileH),
                          QList<QWaylandXdgToplevel::State>());
    }
}

void Compositor::onToplevelCreated(QWaylandXdgToplevel *toplevel,
                                   QWaylandXdgSurface *xdgSurface)
{
    // Add first, then reconfigureTiles (triggered by countChanged) sends
    // each toplevel a configure with the correct per-tile size.
    m_workspaceModel.addWorkspace(xdgSurface, toplevel);

    // If QML hasn't reported a client area yet, reconfigureTiles() is a
    // no-op.  The xdg-shell protocol still requires an initial configure,
    // so send one with {0,0} — the client picks its own size until the
    // first setClientArea() call corrects it.
    if (m_clientArea.isEmpty())
        toplevel->sendConfigure(QSize(0, 0), QList<QWaylandXdgToplevel::State>());

    // Clean up when the surface is destroyed.
    connect(xdgSurface->surface(), &QWaylandSurface::surfaceDestroyed,
            this, [this, xdgSurface]() {
                m_workspaceModel.removeBySurface(xdgSurface);
            });
}
