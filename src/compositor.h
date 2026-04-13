#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QSize>
#include <QWaylandCompositor>
#include <QWaylandKeyboard>
#include <QWaylandOutput>
#include <QWaylandQuickItem>
#include <QWaylandXdgShell>
#include <QQuickWindow>

class Compositor;

struct Workspace {
    int id = 0;
    QWaylandXdgSurface *surface = nullptr;
    QWaylandXdgToplevel *toplevel = nullptr;
};

class WorkspaceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int activeIndex READ activeIndex WRITE activateWorkspace NOTIFY activeIndexChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int pinnedCount READ pinnedCount NOTIFY pinnedCountChanged)

public:
    enum Roles { XdgSurfaceRole = Qt::UserRole + 1, SurfaceIdRole, TitleRole, PinnedRole };

    explicit WorkspaceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int activeIndex() const { return m_activeIndex; }

    int count() const { return m_workspaces.size(); }
    int pinnedCount() const { return m_pinnedIds.size(); }
    const Workspace &workspaceAt(int row) const { return m_workspaces.at(row); }
    QWaylandXdgToplevel *toplevelAt(int row) const;

    // Find the list index for a given stable ID. Returns -1 if not found.
    int findById(int id) const;

    void addWorkspace(QWaylandXdgSurface *surface, QWaylandXdgToplevel *toplevel);
    void removeBySurface(QWaylandXdgSurface *surface);
    void setActiveIndex(int index);

    Q_INVOKABLE void activateWorkspace(int index);

    // Pinning
    Q_INVOKABLE bool pinToSidebar(int id);
    Q_INVOKABLE bool unpinFromSidebar(int id);
    bool isPinned(int id) const;

    // Unpinned workspace helpers (Q_INVOKABLE for QML access)
    Q_INVOKABLE int unpinnedCount() const;
    Q_INVOKABLE int nthUnpinnedIndex(int n) const; // 0-based n -> model row
    Q_INVOKABLE int unpinnedPositionOf(int row) const; // model row -> 0-based unpinned position
    Q_INVOKABLE int nextUnpinnedIndex(int fromRow, int direction) const; // direction: +1 or -1

signals:
    void activeIndexChanged();
    void countChanged();
    void pinnedCountChanged();

private:
    void switchToNearestUnpinned(int fromRow);

    QList<Workspace> m_workspaces;
    QSet<int> m_pinnedIds;
    int m_activeIndex = -1;
    int m_nextId = 1;
};

class CompositorKeyboard : public QWaylandKeyboard {
    Q_OBJECT

public:
    explicit CompositorKeyboard(Compositor *compositor, QWaylandSeat *seat);

    void sendKeyPressEvent(uint code) override;
    void sendKeyReleaseEvent(uint code) override;

private:
    void buildScanCodeMap();

    Compositor *m_compositor;
    uint m_metaLeftCode = 0;
    uint m_metaRightCode = 0;
    QHash<uint, int> m_hotkeyMap; // scan code -> hotkey ID
    QSet<uint> m_consumedKeys;    // release suppression
    bool m_metaHeld = false;
    bool m_shiftHeld = false;
    uint m_shiftLeftCode = 0;
    uint m_shiftRightCode = 0;

    // Hotkey IDs
    static constexpr int HotkeyFocusChat = 0;
    static constexpr int HotkeyWorkspace1 = 1;
    // 2..9 follow sequentially (1..9)
    static constexpr int HotkeyTabForward = 10;
    static constexpr int HotkeyTabBackward = 11;
};

class Compositor : public QWaylandCompositor {
    Q_OBJECT

public:
    explicit Compositor(QQuickWindow *window);

    WorkspaceModel *workspaceModel() { return &m_workspaceModel; }

    Q_INVOKABLE void setClientArea(int width, int height);
    Q_INVOKABLE void forwardMousePress(QWaylandQuickItem *item,
                                       qreal localX, qreal localY,
                                       int button);
    Q_INVOKABLE void forwardMouseRelease(int button);
    Q_INVOKABLE void forwardMouseMove(QWaylandQuickItem *item,
                                      qreal localX, qreal localY);

    void sendPinnedConfigure(int id, int width, int height);

signals:
    void hotkeyFocusChat();
    void hotkeyActivateWorkspace(int index);
    void hotkeyCycleWorkspace(int direction); // +1 forward, -1 backward

protected:
    QWaylandKeyboard *createKeyboardDevice(QWaylandSeat *seat) override;

private slots:
    void onToplevelCreated(QWaylandXdgToplevel *toplevel,
                           QWaylandXdgSurface *xdgSurface);
    void reconfigureTiles();

private:
    QQuickWindow *m_window = nullptr;
    QWaylandOutput *m_output = nullptr;
    QWaylandXdgShell *m_xdgShell = nullptr;
    WorkspaceModel m_workspaceModel;
    QSize m_clientArea{0, 0};
};
