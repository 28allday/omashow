#include "core/punctuation.h"

namespace {

bool opensAfter(QChar before) {
    return before.isNull() || before.isSpace() || before == '(' || before == '[' ||
           before == '{' || before == QChar(0x2014) || before == QChar(0x2013) ||
           before == '-' || before == QChar(0x201c) || before == QChar(0x2018);
}

} // namespace

QString Punctuation::smarten(const QString &text) {
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        const QChar before = out.isEmpty() ? QChar() : out.back();
        if (c == '"') {
            out += opensAfter(before) ? QChar(0x201c) : QChar(0x201d);
            continue;
        }
        if (c == '\'') {
            // Inside a word it is an apostrophe; otherwise it is a quote that
            // faces whichever way it should.
            const bool insideWord = !out.isEmpty() && before.isLetterOrNumber();
            out += insideWord || !opensAfter(before) ? QChar(0x2019) : QChar(0x2018);
            continue;
        }
        if (c == '.' && text.mid(i, 3) == QStringLiteral("...")) {
            out += QChar(0x2026);
            i += 2;
            continue;
        }
        if (c == '-' && text.mid(i, 3) == QStringLiteral("---")) {
            out += QChar(0x2014);
            i += 2;
            continue;
        }
        if (c == '-' && text.mid(i, 2) == QStringLiteral("--")) {
            out += QChar(0x2013);
            i += 1;
            continue;
        }
        out += c;
    }
    return out;
}
