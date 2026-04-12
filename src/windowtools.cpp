#include "windowtools.h"
#include "compositor.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QWaylandXdgSurface>

QList<ToolHost::ToolSpec> windowToolSpecs(WorkspaceModel *model)
{
    QList<ToolHost::ToolSpec> specs;

    // list_windows — no args, returns [{id, title, active}, ...]
    {
        ToolHost::ToolSpec spec;
        spec.name = QStringLiteral("list_windows");
        spec.description = QStringLiteral(
            "List all open windows with their IDs and titles.");

        QJsonObject parameters;
        parameters[QStringLiteral("type")] = QStringLiteral("object");
        parameters[QStringLiteral("properties")] = QJsonObject();
        spec.parameters = parameters;

        spec.handler = [model](const QJsonObject &, QString *) -> QJsonValue {
            QJsonArray windows;
            for (int i = 0; i < model->count(); ++i) {
                const Workspace &ws = model->workspaceAt(i);
                QJsonObject entry;
                entry[QStringLiteral("id")] = ws.id;
                entry[QStringLiteral("title")] =
                    ws.toplevel ? ws.toplevel->title() : QString();
                entry[QStringLiteral("active")] = (i == model->activeIndex());
                windows.append(entry);
            }
            return windows;
        };
        specs.append(std::move(spec));
    }

    // close_window(id) — sends xdg close request
    {
        ToolHost::ToolSpec spec;
        spec.name = QStringLiteral("close_window");
        spec.description = QStringLiteral(
            "Close a window by its ID. Use list_windows to find IDs.");

        QJsonObject idProp;
        idProp[QStringLiteral("type")] = QStringLiteral("integer");
        idProp[QStringLiteral("description")] = QStringLiteral(
            "The window ID to close.");

        QJsonObject properties;
        properties[QStringLiteral("id")] = idProp;

        QJsonObject parameters;
        parameters[QStringLiteral("type")] = QStringLiteral("object");
        parameters[QStringLiteral("properties")] = properties;
        QJsonArray required;
        required.append(QStringLiteral("id"));
        parameters[QStringLiteral("required")] = required;
        spec.parameters = parameters;

        spec.handler = [model](const QJsonObject &args,
                               QString *error) -> QJsonValue {
            const int id = args.value(QStringLiteral("id")).toInt(-1);
            if (id < 0) {
                if (error)
                    *error = QStringLiteral("missing or invalid required arg: id");
                return {};
            }
            int idx = model->findById(id);
            if (idx < 0) {
                if (error)
                    *error = QStringLiteral("no window with id %1").arg(id);
                return {};
            }
            QWaylandXdgToplevel *tl = model->toplevelAt(idx);
            if (!tl) {
                if (error)
                    *error = QStringLiteral("window %1 has no toplevel").arg(id);
                return {};
            }
            tl->sendClose();
            QJsonObject ok;
            ok[QStringLiteral("ok")] = true;
            ok[QStringLiteral("closed")] = id;
            return ok;
        };
        specs.append(std::move(spec));
    }

    // switch_workspace(id) — sets the active workspace
    {
        ToolHost::ToolSpec spec;
        spec.name = QStringLiteral("switch_workspace");
        spec.description = QStringLiteral(
            "Switch to a workspace/window by its ID. Use list_windows to find IDs.");

        QJsonObject idProp;
        idProp[QStringLiteral("type")] = QStringLiteral("integer");
        idProp[QStringLiteral("description")] = QStringLiteral(
            "The workspace/window ID to switch to.");

        QJsonObject properties;
        properties[QStringLiteral("id")] = idProp;

        QJsonObject parameters;
        parameters[QStringLiteral("type")] = QStringLiteral("object");
        parameters[QStringLiteral("properties")] = properties;
        QJsonArray required;
        required.append(QStringLiteral("id"));
        parameters[QStringLiteral("required")] = required;
        spec.parameters = parameters;

        spec.handler = [model](const QJsonObject &args,
                               QString *error) -> QJsonValue {
            const int id = args.value(QStringLiteral("id")).toInt(-1);
            if (id < 0) {
                if (error)
                    *error = QStringLiteral("missing or invalid required arg: id");
                return {};
            }
            int idx = model->findById(id);
            if (idx < 0) {
                if (error)
                    *error = QStringLiteral("no window with id %1").arg(id);
                return {};
            }
            model->setActiveIndex(idx);
            QJsonObject ok;
            ok[QStringLiteral("ok")] = true;
            ok[QStringLiteral("switched_to")] = id;
            return ok;
        };
        specs.append(std::move(spec));
    }

    return specs;
}
