#include "io/zip.h"

#include <QDataStream>
#include <QVector>

#include <zlib.h>

namespace {

constexpr quint32 kLocalSignature = 0x04034b50;
constexpr quint32 kCentralSignature = 0x02014b50;
constexpr quint32 kEndSignature = 0x06054b50;
constexpr quint16 kMethodStore = 0;
constexpr quint16 kMethodDeflate = 8;
// A fixed DOS timestamp (1 Jan 2020, 00:00) keeps output byte-identical between
// saves of the same document. Real mtimes would make every save differ.
constexpr quint16 kDosTime = 0;
constexpr quint16 kDosDate = 0x5021;

QByteArray deflateRaw(const QByteArray &input) {
    z_stream stream = {};
    // windowBits -15 selects a raw deflate stream, which is what zip stores —
    // zlib's default would add a 2-byte header that no unzip expects here.
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK)
        return QByteArray();

    QByteArray out;
    out.resize(int(deflateBound(&stream, uLong(input.size()))));
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    stream.avail_in = uInt(input.size());
    stream.next_out = reinterpret_cast<Bytef *>(out.data());
    stream.avail_out = uInt(out.size());

    const int result = deflate(&stream, Z_FINISH);
    const int written = int(stream.total_out);
    deflateEnd(&stream);
    if (result != Z_STREAM_END)
        return QByteArray();

    out.resize(written);
    return out;
}

QByteArray inflateRaw(const QByteArray &input, quint32 expectedSize) {
    z_stream stream = {};
    if (inflateInit2(&stream, -15) != Z_OK)
        return QByteArray();

    QByteArray out;
    out.resize(int(expectedSize));
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    stream.avail_in = uInt(input.size());
    stream.next_out = reinterpret_cast<Bytef *>(out.data());
    stream.avail_out = uInt(out.size());

    const int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (result != Z_STREAM_END)
        return QByteArray();
    return out;
}

void put16(QByteArray &out, quint16 value) {
    out.append(char(value & 0xFF));
    out.append(char((value >> 8) & 0xFF));
}

void put32(QByteArray &out, quint32 value) {
    out.append(char(value & 0xFF));
    out.append(char((value >> 8) & 0xFF));
    out.append(char((value >> 16) & 0xFF));
    out.append(char((value >> 24) & 0xFF));
}

quint16 get16(const QByteArray &raw, int offset) {
    return quint16(quint8(raw.at(offset))) | (quint16(quint8(raw.at(offset + 1))) << 8);
}

quint32 get32(const QByteArray &raw, int offset) {
    return quint32(quint8(raw.at(offset)))
         | (quint32(quint8(raw.at(offset + 1))) << 8)
         | (quint32(quint8(raw.at(offset + 2))) << 16)
         | (quint32(quint8(raw.at(offset + 3))) << 24);
}

} // namespace

QByteArray Zip::write(const QVector<Entry> &entries) {
    QByteArray out;
    struct Record {
        QString name;
        quint32 crc;
        quint32 compressedSize;
        quint32 size;
        quint16 method;
        quint32 offset;
    };
    QVector<Record> records;

    for (const Entry &entry : entries) {
        const QByteArray name = entry.name.toUtf8();
        QByteArray payload = entry.data;
        quint16 method = kMethodStore;

        if (entry.compress && !payload.isEmpty()) {
            const QByteArray deflated = deflateRaw(payload);
            // Already-compressed data can deflate larger than it started.
            if (!deflated.isEmpty() && deflated.size() < payload.size()) {
                payload = deflated;
                method = kMethodDeflate;
            }
        }

        Record record;
        record.name = entry.name;
        record.crc = quint32(crc32(0, reinterpret_cast<const Bytef *>(entry.data.constData()),
                                   uInt(entry.data.size())));
        record.compressedSize = quint32(payload.size());
        record.size = quint32(entry.data.size());
        record.method = method;
        record.offset = quint32(out.size());
        records.append(record);

        put32(out, kLocalSignature);
        put16(out, 20);            // version needed
        put16(out, 0);             // flags
        put16(out, method);
        put16(out, kDosTime);
        put16(out, kDosDate);
        put32(out, record.crc);
        put32(out, record.compressedSize);
        put32(out, record.size);
        put16(out, quint16(name.size()));
        put16(out, 0);             // extra length
        out.append(name);
        out.append(payload);
    }

    const quint32 centralStart = quint32(out.size());
    for (const Record &record : records) {
        const QByteArray name = record.name.toUtf8();
        put32(out, kCentralSignature);
        put16(out, 20);            // version made by
        put16(out, 20);            // version needed
        put16(out, 0);             // flags
        put16(out, record.method);
        put16(out, kDosTime);
        put16(out, kDosDate);
        put32(out, record.crc);
        put32(out, record.compressedSize);
        put32(out, record.size);
        put16(out, quint16(name.size()));
        put16(out, 0);             // extra
        put16(out, 0);             // comment
        put16(out, 0);             // disk
        put16(out, 0);             // internal attrs
        put32(out, 0);             // external attrs
        put32(out, record.offset);
        out.append(name);
    }
    const quint32 centralSize = quint32(out.size()) - centralStart;

    put32(out, kEndSignature);
    put16(out, 0);                 // this disk
    put16(out, 0);                 // disk with central directory
    put16(out, quint16(records.size()));
    put16(out, quint16(records.size()));
    put32(out, centralSize);
    put32(out, centralStart);
    put16(out, 0);                 // comment length
    return out;
}

Zip::Reader::Reader(const QByteArray &raw) : m_raw(raw) {
    // Find the end-of-central-directory record by scanning back from the end.
    // There is no comment in anything this writes, but a file from elsewhere
    // may carry one.
    if (m_raw.size()>512*1024*1024) { m_error=QStringLiteral("archive exceeds 512 MB"); return; }
    quint64 expandedTotal=0;
    int end = -1;
    for (int i = m_raw.size() - 22; i >= 0 && i >= m_raw.size() - 22 - 65535; --i) {
        if (get32(m_raw, i) == kEndSignature) {
            end = i;
            break;
        }
    }
    if (end < 0) {
        m_error = QStringLiteral("not a zip container");
        return;
    }

    const quint16 count = get16(m_raw, end + 10);
    quint32 offset = get32(m_raw, end + 16);

    for (quint16 i = 0; i < count; ++i) {
        if (quint64(offset) + 46 > quint64(m_raw.size()) || get32(m_raw, int(offset)) != kCentralSignature) {
            m_error = QStringLiteral("damaged central directory");
            return;
        }
        const quint16 method = get16(m_raw, int(offset) + 10);
        const quint32 crc = get32(m_raw, int(offset) + 16);
        const quint32 compressedSize = get32(m_raw, int(offset) + 20);
        const quint32 size = get32(m_raw, int(offset) + 24);
        const quint16 nameLength = get16(m_raw, int(offset) + 28);
        const quint16 extraLength = get16(m_raw, int(offset) + 30);
        const quint16 commentLength = get16(m_raw, int(offset) + 32);
        const quint32 localOffset = get32(m_raw, int(offset) + 42);
        expandedTotal += size;
        if (size>64*1024*1024 || expandedTotal>512*1024*1024 ||
            quint64(offset)+46+nameLength+extraLength+commentLength>quint64(m_raw.size()) ||
            (method!=kMethodStore && method!=kMethodDeflate)) {
            m_error=QStringLiteral("unsupported or oversized archive member"); return;
        }
        const QString name = QString::fromUtf8(m_raw.mid(int(offset) + 46, nameLength));

        // A member name is never allowed to climb out of the bundle: an archive
        // from elsewhere is untrusted input.
        if (name.isEmpty() || m_entries.contains(name) || name.contains(QStringLiteral("..")) || name.startsWith(QLatin1Char('/'))) {
            m_error = QStringLiteral("unsafe member name");
            return;
        }

        if (quint64(localOffset) + 30 > quint64(m_raw.size())
                || get32(m_raw, int(localOffset)) != kLocalSignature) {
            m_error = QStringLiteral("damaged member");
            return;
        }
        const quint16 localNameLength = get16(m_raw, int(localOffset) + 26);
        const quint16 localExtraLength = get16(m_raw, int(localOffset) + 28);
        const int dataStart = int(localOffset) + 30 + localNameLength + localExtraLength;
        if (quint64(dataStart) + compressedSize > quint64(m_raw.size())) {
            m_error = QStringLiteral("truncated member");
            return;
        }

        const QByteArray stored = m_raw.mid(dataStart, int(compressedSize));
        QByteArray data = method == kMethodDeflate ? inflateRaw(stored, size) : stored;
        if (quint32(data.size()) != size) {
            m_error = QStringLiteral("member did not decompress");
            return;
        }
        if (quint32(crc32(0, reinterpret_cast<const Bytef *>(data.constData()),
                          uInt(data.size()))) != crc) {
            m_error = QStringLiteral("member failed its checksum");
            return;
        }

        m_entries.insert(name, data);
        offset += 46u + nameLength + extraLength + commentLength;
    }
    m_valid = true;
}

QStringList Zip::Reader::names() const {
    QStringList names = m_entries.keys();
    names.sort();
    return names;
}

QByteArray Zip::Reader::read(const QString &name) const {
    return m_entries.value(name);
}
