#pragma once

#include "toolhost.h"

#include <QList>

class WorkspaceModel;

QList<ToolHost::ToolSpec> windowToolSpecs(WorkspaceModel *model);
