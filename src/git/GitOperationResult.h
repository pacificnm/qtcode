#pragma once

#include <QString>

namespace qtcode::git {

struct GitOperationResult
{
    bool success = false;
    QString errorMessage;
    QString standardOutput;
};

} // namespace qtcode::git
