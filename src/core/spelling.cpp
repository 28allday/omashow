#include "core/spelling.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QStringEncoder>

#include <hunspell/hunspell.hxx>

#include <memory>

namespace {

// A dictionary, kept open: loading one costs tens of milliseconds and the
// review checks the whole deck every time it is asked.
struct Loaded {
    std::unique_ptr<Hunspell> hunspell;
    QStringEncoder to;
    QStringDecoder from;
};

QMutex &lock() {
    static QMutex mutex;
    return mutex;
}
QHash<QString, std::shared_ptr<Loaded>> &loaded() {
    static QHash<QString, std::shared_ptr<Loaded>> table;
    return table;
}

QString tidyLanguage(const QString &language) {
    QString tidy = language.trimmed();
    tidy.replace('-', '_');
    // The language comes from the deck; it names a file, so it must look like
    // a language tag and nothing else ("en_GB", "de", "sr_Latn_RS").
    static const QRegularExpression tag(QStringLiteral("^[A-Za-z]{2,3}(_[A-Za-z0-9]{2,8}){0,3}$"));
    return tag.match(tidy).hasMatch() ? tidy : QString();
}

std::shared_ptr<Loaded> open(const QString &language) {
    const auto path = Spelling::dictionaryFor(language);
    if (path.isEmpty()) return {};
    QMutexLocker held(&lock());
    if (const auto found = loaded().value(path)) return found;
    const auto affix = path + QStringLiteral(".aff");
    const auto words = path + QStringLiteral(".dic");
    auto opened = std::make_shared<Loaded>();
    opened->hunspell = std::make_unique<Hunspell>(affix.toLocal8Bit().constData(),
                                                  words.toLocal8Bit().constData());
    const QByteArray encoding = opened->hunspell->get_dic_encoding();
    opened->to = QStringEncoder(encoding.isEmpty() ? QByteArray("UTF-8") : encoding);
    opened->from = QStringDecoder(encoding.isEmpty() ? QByteArray("UTF-8") : encoding);
    if (!opened->to.isValid() || !opened->from.isValid()) {
        opened->to = QStringEncoder(QStringEncoder::Utf8);
        opened->from = QStringDecoder(QStringDecoder::Utf8);
    }
    loaded().insert(path, opened);
    return opened;
}

} // namespace

QStringList Spelling::searchPaths() {
    const auto set = QProcessEnvironment::systemEnvironment().value(
        QStringLiteral("OMASHOW_DICTIONARIES"));
    if (!set.isEmpty()) return set.split(':', Qt::SkipEmptyParts);
    QStringList paths{QStringLiteral("/usr/share/hunspell"),
                      QStringLiteral("/usr/share/myspell"),
                      QStringLiteral("/usr/share/myspell/dicts"),
                      QStringLiteral("/usr/local/share/hunspell")};
    for (const auto &data : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        paths.append(data + QStringLiteral("/hunspell"));
    return paths;
}

QStringList Spelling::installed() {
    QStringList languages;
    for (const auto &folder : searchPaths()) {
        QDir directory(folder);
        if (!directory.exists()) continue;
        for (const auto &entry : directory.entryList({QStringLiteral("*.dic")}, QDir::Files)) {
            const auto language = QFileInfo(entry).completeBaseName();
            if (!languages.contains(language)) languages.append(language);
        }
    }
    languages.sort();
    return languages;
}

QString Spelling::dictionaryFor(const QString &language) {
    const auto wanted = tidyLanguage(language);
    if (wanted.isEmpty()) return {};
    const auto haveBoth = [](const QString &base) {
        return QFileInfo::exists(base + QStringLiteral(".dic")) &&
               QFileInfo::exists(base + QStringLiteral(".aff"));
    };
    for (const auto &folder : searchPaths()) {
        const auto base = folder + '/' + wanted;
        if (haveBoth(base)) return base;
    }
    // A dictionary for the same language in another place is better than none:
    // en_GB spelling checked against en_US is worth saying something about, and
    // the review says which dictionary answered.
    // Packages install one real dictionary and link it under many regional
    // names (hunspell-en_gb: en_GB and en_AG, en_BS, … all lead to
    // en_GB-large), so the first name alphabetically is rarely the one to
    // report. Prefer a plain name that its real file is named after, then any
    // plain name, then whatever there is.
    const auto stem = wanted.section('_', 0, 0);
    QString plainName, anything;
    for (const auto &folder : searchPaths()) {
        QDir directory(folder);
        if (!directory.exists()) continue;
        for (const auto &entry : directory.entryList({stem + QStringLiteral("*.dic")}, QDir::Files)) {
            const auto name = QFileInfo(entry).completeBaseName();
            const auto base = folder + '/' + name;
            if (!haveBoth(base)) continue;
            const bool plain = !name.contains('-');
            const auto real = QFileInfo(QFileInfo(base + QStringLiteral(".dic")).canonicalFilePath())
                                  .completeBaseName();
            if (plain && real.startsWith(name)) return base;
            if (plain && plainName.isEmpty()) plainName = base;
            if (anything.isEmpty()) anything = base;
        }
    }
    return plainName.isEmpty() ? anything : plainName;
}

bool Spelling::available(const QString &language) {
    return !dictionaryFor(language).isEmpty();
}

QVector<Spelling::Word> Spelling::check(const QString &text, const QString &language,
                                        const QStringList &known) {
    QVector<Word> wrong;
    const auto dictionary = open(language);
    if (!dictionary || text.isEmpty()) return wrong;
    QStringList taught;
    for (const auto &word : known) taught.append(word.toCaseFolded());
    // Words, apostrophes and all, but never numbers, web addresses or the
    // machinery of an equation.
    static const QRegularExpression pattern(
        QStringLiteral("[\\p{L}][\\p{L}\\p{M}'\\x{2019}-]*"));
    auto matches = pattern.globalMatch(text);
    QMutexLocker held(&lock());
    while (matches.hasNext()) {
        const auto match = matches.next();
        QString word = match.captured();
        while (word.endsWith('-') || word.endsWith('\'') ||
               word.endsWith(QChar(0x2019)))
            word.chop(1);
        if (word.size() < 2) continue;
        if (taught.contains(word.toCaseFolded())) continue;
        // Anything that looks like an address or a file is not prose.
        const int after = match.capturedEnd();
        if (after < text.size() && (text.at(after) == '@' || text.at(after) == '/' ||
                                    text.at(after) == ':' || text.at(after) == '.')) {
            const QString rest = text.mid(after, 3);
            if (rest.startsWith(QStringLiteral("://")) || text.at(after) == '@') continue;
        }
        const QByteArray encoded = dictionary->to.encode(word);
        if (dictionary->hunspell->spell(std::string(encoded.constData(), encoded.size())))
            continue;
        wrong.append({match.capturedStart(), word.size(), word});
    }
    return wrong;
}

QStringList Spelling::suggest(const QString &word, const QString &language, int limit) {
    QStringList offered;
    const auto dictionary = open(language);
    if (!dictionary || word.isEmpty()) return offered;
    QMutexLocker held(&lock());
    const QByteArray encoded = dictionary->to.encode(word);
    for (const auto &suggestion :
         dictionary->hunspell->suggest(std::string(encoded.constData(), encoded.size()))) {
        offered.append(dictionary->from.decode(
            QByteArray(suggestion.data(), qsizetype(suggestion.size()))));
        if (offered.size() >= limit) break;
    }
    return offered;
}
