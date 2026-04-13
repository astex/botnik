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
    case PinnedRole:
        return m_pinnedIds.contains(ws.id);
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
        {PinnedRole, "pinned"},
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

    bool wasPinned = m_pinnedIds.remove(m_workspaces.at(idx).id);

    beginRemoveRows(QModelIndex(), idx, idx);
    m_workspaces.removeAt(idx);
    endRemoveRows();

    if (wasPinned)
        emit pinnedCountChanged();

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

// --- Pinning ---

bool WorkspaceModel::pinToSidebar(int id)
{
    int idx = findById(id);
    if (idx < 0 || m_pinnedIds.contains(id))
        return false;

    m_pinnedIds.insert(id);
    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {PinnedRole});
    emit pinnedCountChanged();

    // If the pinned surface was active, switch to nearest unpinned.
    if (idx == m_activeIndex)
        switchToNearestUnpinned(idx);

    return true;
}

bool WorkspaceModel::unpinFromSidebar(int id)
{
    int idx = findById(id);
    if (idx < 0 || !m_pinnedIds.contains(id))
        return false;

    m_pinnedIds.remove(id);
    QModelIndex mi = index(idx);
    emit dataChanged(mi, mi, {PinnedRole});
    emit pinnedCountChanged();

    // If no unpinned workspace is active, activate the newly unpinned one.
    if (m_activeIndex < 0 || m_pinnedIds.contains(m_workspaces.at(m_activeIndex).id))
        setActiveIndex(idx);

    return true;
}

bool WorkspaceModel::isPinned(int id) const
{
    return m_pinnedIds.contains(id);
}

int WorkspaceModel::unpinnedCount() const
{
    int count = 0;
    for (const auto &ws : m_workspaces) {
        if (!m_pinnedIds.contains(ws.id))
            ++count;
    }
    return count;
}

int WorkspaceModel::nthUnpinnedIndex(int n) const
{
    int seen = 0;
    for (int i = 0; i < m_workspaces.size(); ++i) {
        if (!m_pinnedIds.contains(m_workspaces.at(i).id)) {
            if (seen == n)
                return i;
            ++seen;
        }
    }
    return -1;
}

int WorkspaceModel::unpinnedPositionOf(int row) const
{
    if (row < 0 || row >= m_workspaces.size())
        return -1;
    if (m_pinnedIds.contains(m_workspaces.at(row).id))
        return -1;
    int pos = 0;
    for (int i = 0; i < row; ++i) {
        if (!m_pinnedIds.contains(m_workspaces.at(i).id))
            ++pos;
    }
    return pos;
}

int WorkspaceModel::nextUnpinnedIndex(int fromRow, int direction) const
{
    const int n = m_workspaces.size();
    if (n == 0)
        return -1;
    for (int step = 1; step < n; ++step) {
        int candidate = (fromRow + (step * direction) % n + n) % n;
        if (!m_pinnedIds.contains(m_workspaces.at(candidate).id))
            return candidate;
    }
    return -1;
}

void WorkspaceModel::switchToNearestUnpinned(int fromRow)
{
    // Prefer the one after, fall back to before, then -1.
    const int n = m_workspaces.size();
    for (int i = fromRow + 1; i < n; ++i) {
        if (!m_pinnedIds.contains(m_workspaces.at(i).id)) {
            setActiveIndex(i);
            return;
        }
    }
    for (int i = fromRow - 1; i >= 0; --i) {
        if (!m_pinnedIds.contains(m_workspaces.at(i).id)) {
            setActiveIndex(i);
            return;
        }
    }
    setActiveIndex(-1);
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
    m_shiftLeftCode = keyToScanCode(Qt::Key_Shift);
    m_shiftRightCode = keyToScanCode(Qt::Key_Shift); // same code for both

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

    // Meta+Tab -> cycle workspaces (direction depends on Shift)
    uint tabCode = keyToScanCode(Qt::Key_Tab);
    if (tabCode)
        m_hotkeyMap.insert(tabCode, HotkeyTabForward);
}

void CompositorKeyboard::sendKeyPressEvent(uint code)
{
    // Track Meta modifier.
    if (code == m_metaLeftCode || code == m_metaRightCode) {
        m_metaHeld = true;
        QWaylandKeyboard::sendKeyPressEvent(code);
        return;
    }

    // Track Shift modifier.
    if (code == m_shiftLeftCode || code == m_shiftRightCode) {
        m_shiftHeld = true;
        QWaylandKeyboard::sendKeyPressEvent(code);
        return;
    }

    if (m_metaHeld) {
        auto it = m_hotkeyMap.find(code);
        if (it != m_hotkeyMap.end()) {
            int id = it.value();
            if (id == HotkeyFocusChat) {
                emit m_compositor->hotkeyFocusChat();
            } else if (id == HotkeyTabForward) {
                int direction = m_shiftHeld ? -1 : 1;
                emit m_compositor->hotkeyCycleWorkspace(direction);
            } else {
                // HotkeyWorkspace1..9 -> 0-based unpinned position
                int wsIndex = id - HotkeyWorkspace1;
                emit m_compositor->hotkeyActivateWorkspace(wsIndex);
            }
            m_consumedKeys.insert(code); // Suppress release
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
    if (code == m_shiftLeftCode || code == m_shiftRightCode) {
        m_shiftHeld = false;
    }

    // Suppress release for keys we consumed on press.
    if (m_consumedKeys.remove(code))
        return;

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

    // Virtual-desktop mode: every unpinned surface gets the full client area.
    for (int i = 0; i < n; ++i) {
        auto *tl = m_workspaceModel.toplevelAt(i);
        if (!tl)
            continue;
        const auto &ws = m_workspaceModel.workspaceAt(i);
        if (m_workspaceModel.isPinned(ws.id))
            continue; // Pinned surfaces get their own configure.
        tl->sendConfigure(m_clientArea,
                          QList<QWaylandXdgToplevel::State>());
    }
}

void Compositor::sendPinnedConfigure(int id, int width, int height)
{
    int idx = m_workspaceModel.findById(id);
    if (idx < 0)
        return;
    auto *tl = m_workspaceModel.toplevelAt(idx);
    if (tl)
        tl->sendConfigure(QSize(width, height),
                          QList<QWaylandXdgToplevel::State>());
}

void Compositor::onToplevelCreated(QWaylandXdgToplevel *toplevel,
                                   QWaylandXdgSurface *xdgSurface)
{
    // Add first, then reconfigureTiles (triggered by countChanged) sends
    // each toplevel a configure with the correct per-tile size.
    m_workspaceModel.addWorkspace(xdgSurface, toplevel);

    // Auto-pin the clock to the sidebar so the default view is agent-only.
    if (toplevel) {
        int wsId = m_workspaceModel.workspaceAt(m_workspaceModel.count() - 1).id;
        auto tryAutoPin = [this, toplevel, wsId]() {
            if (toplevel->title() == QStringLiteral("botnik-clock") ||
                toplevel->title() == QStringLiteral("botnik-battery"))
                m_workspaceModel.pinToSidebar(wsId);
        };
        tryAutoPin(); // Title may already be set.
        connect(toplevel, &QWaylandXdgToplevel::titleChanged,
                this, tryAutoPin);
    }

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
