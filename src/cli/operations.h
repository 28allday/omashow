#pragma once

// Running one backend operation, named as a string.
//
// The command line and the interface call the same invokables: the brief's rule
// is one code path, and this is how the command line keeps it. Rather than a
// hand-written wrapper per action — which would be out of date the day someone
// adds a feature to the interface — an operation is looked up on Backend's own
// meta-object by name, its arguments converted from JSON to the types the
// method asks for, and the result handed back as JSON.
//
// That means everything the interface can do, the command line can do, and
// `omashow ops` can describe the whole surface without anybody maintaining a
// list. Anything that would open a dialog or wait on the desktop is refused by
// name, with the synchronous alternative given where there is one.

#include <QStringList>
#include <QVariant>
#include <QVariantMap>

class Backend;

namespace Cli {

struct OpResult {
    bool ok = false;
    QString error;
    QVariant value;       // what the method returned, if anything
};

// Every operation, with its arguments and what it gives back. `filter` narrows
// by name, case-insensitively.
QVariantList describeOperations(const QString &filter = QString());

// `op` is {"op": name, "args": [...] | {...}, "select": id?, "slide": n?}.
OpResult runOperation(Backend &backend, const QVariantMap &op);

// Why an operation cannot be run here, or empty if it can.
QString refusal(const QString &name);
} // namespace Cli
