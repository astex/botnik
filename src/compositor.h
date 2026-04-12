#pragma once

#include <QAbstractListModel>
#include <QSize>
#include <QWaylandCompositor>
#include <QWaylandOutput>
#include <QWaylandXdgShell>
#include <QQuickWindow>

struct Workspace {
    int id = 0;
    QWaylandXdgSurface *surface = nullptr;
    QWaylandXdgToplevel *toplevel = nullptr;
};

class WorkspaceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIndexChanged)

public:
    enum Roles { XdgSurfaceRole = Qt::UserRole + 1, SurfaceIdRole };

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

signals:
    void activeIndexChanged();

private:
    QList<Workspace> m_workspaces;
    int m_activeIndex = -1;
    int m_nextId = 1;
};

class Compositor : public QWaylandCompositor {
    Q_OBJECT

public:
    explicit Compositor(QQuickWindow *window);

    WorkspaceModel *workspaceModel() { return &m_workspaceModel; }

    Q_INVOKABLE void setClientArea(int width, int height);

signals:
    void hotkeyFocusChat();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onToplevelCreated(QWaylandXdgToplevel *toplevel,
                           QWaylandXdgSurface *xdgSurface);

private:
    QQuickWindow *m_window = nullptr;
    QWaylandOutput *m_output = nullptr;
    QWaylandXdgShell *m_xdgShell = nullptr;
    WorkspaceModel m_workspaceModel;
    QSize m_clientArea{0, 0};
};
