#pragma once

// The command line: OmaShow with no window in front of it.
//
// Six verbs — new, inspect, apply, export, review, ops — each reading and
// writing JSON, so something that cannot see the screen can still make a deck,
// change one, look at what it has made and export it. They run the same backend
// the interface does, which is the whole point: there is no command-line subset
// of the app, and nothing an agent does here is a second implementation of what
// a person does there.

#include <QStringList>

class Backend;

namespace Cli {

// Whether the first argument names a verb rather than a file to open.
bool isVerb(const QString &word);

// Runs it. Result JSON goes to stdout, anything to read goes to stderr, and the
// exit code is 0 for done and 1 for not.
int run(Backend &backend, const QStringList &arguments);
} // namespace Cli
