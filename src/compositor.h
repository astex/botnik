#pragma once

#include <QAbstractListModel>
#include <QWaylandCompositor>
#include <QWaylandOutput>
#include <QWaylandXdgShell>
#include <QQuickWindow>

class SurfaceModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles { XdgSurfaceRole = Qt::UserRole + 1 };

    explicit SurfaceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void addSurface(QWaylandXdgSurface *surface);
    void removeSurface(QWaylandXdgSurface *surface);

private:
    QList<QWaylandXdgSurface *> m_surfaces;
};

class Compositor : public QWaylandCompositor {
    Q_OBJECT

public:
    explicit Compositor(QQuickWindow *window);

    SurfaceModel *surfaceModel() { return &m_surfaceModel; }

private slots:
    void onToplevelCreated(QWaylandXdgToplevel *toplevel,
                           QWaylandXdgSurface *xdgSurface);

private:
    QWaylandOutput *m_output = nullptr;
    QWaylandXdgShell *m_xdgShell = nullptr;
    SurfaceModel m_surfaceModel;
};
