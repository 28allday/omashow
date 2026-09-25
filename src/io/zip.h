#pragma once

// A minimal zip container: store and deflate, no zip64, no encryption.
//
// Written in-house rather than pulling in a zip library, because every new
// entry in the package's depends=() is a cost paid by every install, and zlib
// is already underneath Qt. A deck bundle is a handful of small JSON members
// plus assets that are already compressed, which is the easy half of the format.
//
// Output is deterministic: entries carry a fixed timestamp, so saving the same
// document twice produces byte-identical files. That keeps a deck diffable and
// makes "did this actually change?" answerable.

#include <QByteArray>
#include <QHash>
#include <QStringList>

namespace Zip {

struct Entry {
    QString name;
    QByteArray data;
    bool compress = true;
};

// Empty when the archive would not fit the format (no zip64: 4 GB).
QByteArray write(const QVector<Entry> &entries);

// The most a deck may be, so that whatever is written can be read again.
constexpr qint64 kMaxArchiveBytes = 512LL * 1024 * 1024;

class Reader {
public:
    explicit Reader(const QByteArray &raw);

    bool isValid() const { return m_valid; }
    QString error() const { return m_error; }
    bool contains(const QString &name) const { return m_entries.contains(name); }
    QStringList names() const;
    QByteArray read(const QString &name) const;

private:
    QByteArray m_raw;
    QHash<QString, QByteArray> m_entries;
    bool m_valid = false;
    QString m_error;
};

} // namespace Zip
