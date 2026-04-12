#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QSize>
#include <QWaylandCompositor>
#include <QWaylandKeyboard>
#include <QWaylandOutput>
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

public:
    enum Roles { XdgSurfaceRole = Qt::UserRole + 1, SurfaceIdRole, TitleRole };

    explicit WorkspaceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int activeIndex() const { return m_activeIndex; }

    int count() const { return m_workspaces.size(); }
    const Workspace &workspaceAt(int row) const { return m_workspaces.at(row); }
    QWaylandXdgToplevel *toplevelAt(int row) const;

    // Find the list index for a given stable ID. Returns -1 if not found.
    int findById(int id) const;

    void addWorkspace(QWaylandXdgSurface *surface, QWaylandXdgToplevel *toplevel);
    void removeBySurface(QWaylandXdgSurface *surface);
    void setActiveIndex(int index);

    Q_INVOKABLE void activateWorkspace(int index);

signals:
    void activeIndexChanged();
    void countChanged();

private:
    QList<Workspace> m_workspaces;
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
    bool m_metaHeld = false;

    // Hotkey IDs
    static constexpr int HotkeyFocusChat = 0;
    static constexpr int HotkeyWorkspace1 = 1;
    // 2..9 follow sequentially
};

class Compositor : public QWaylandCompositor {
    Q_OBJECT

public:
    explicit Compositor(QQuickWindow *window);

    WorkspaceModel *workspaceModel() { return &m_workspaceModel; }

    Q_INVOKABLE void setClientArea(int width, int height);

signals:
    void hotkeyFocusChat();
    void hotkeyActivateWorkspace(int index);

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
