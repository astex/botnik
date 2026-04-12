#pragma once

#include <QAbstractListModel>
#include <QSize>
#include <QWaylandCompositor>
#include <QWaylandOutput>
#include <QWaylandXdgShell>
#include <QQuickWindow>

struct Workspace {
    QWaylandXdgSurface *surface = nullptr;
    QWaylandXdgToplevel *toplevel = nullptr;
};

class WorkspaceModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeIndexChanged)

public:
    enum Roles { XdgSurfaceRole = Qt::UserRole + 1 };

    explicit WorkspaceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int activeIndex() const { return m_activeIndex; }

    int count() const { return m_workspaces.size(); }
    QWaylandXdgToplevel *toplevelAt(int row) const;

    void addWorkspace(QWaylandXdgSurface *surface, QWaylandXdgToplevel *toplevel);
    void removeBySurface(QWaylandXdgSurface *surface);

signals:
    void activeIndexChanged();

private:
    void setActiveIndex(int index);

    QList<Workspace> m_workspaces;
    int m_activeIndex = -1;
};

class Compositor : public QWaylandCompositor {
    Q_OBJECT

public:
    explicit Compositor(QQuickWindow *window);

    WorkspaceModel *workspaceModel() { return &m_workspaceModel; }

    Q_INVOKABLE void setClientArea(int width, int height);

private slots:
    void onToplevelCreated(QWaylandXdgToplevel *toplevel,
                           QWaylandXdgSurface *xdgSurface);

private:
    QWaylandOutput *m_output = nullptr;
    QWaylandXdgShell *m_xdgShell = nullptr;
    WorkspaceModel m_workspaceModel;
    QSize m_clientArea{0, 0};
};
