#pragma once

// Checking the words on the slides.
//
// A deck says which language it is written in; a box or a stretch of text can
// say something else. Dictionaries are the ones already installed on the
// computer — Hunspell's, the same files every other application here uses —
// because a spelling dictionary is a big thing to carry around in a deck and a
// bigger thing to keep up to date.
//
// Nothing is corrected automatically: a misspelling is a finding in the review,
// with what the dictionary would suggest, and a word the deck should know is
// added to the deck rather than to the computer.

#include <QString>
#include <QStringList>
#include <QVector>

namespace Spelling {

struct Word {
    int start = 0;
    int length = 0;
    QString word;
};

// The languages this computer has dictionaries for, as "en_GB" and the like.
QStringList installed();
// Whether `language` (or the closest dictionary to it, "en_GB" → "en_US") can
// be checked at all. An empty language means the deck's, resolved by the caller.
bool available(const QString &language);
// The dictionary that will actually be used for `language`, or empty.
QString dictionaryFor(const QString &language);

// Words in `text` the dictionary does not know, in the order they appear.
// `known` are the words the deck has been taught, compared without case.
QVector<Word> check(const QString &text, const QString &language,
                    const QStringList &known = {});
// What the dictionary would put instead, best first, at most `limit`.
QStringList suggest(const QString &word, const QString &language, int limit = 4);

// Where dictionaries are looked for. OMASHOW_DICTIONARIES overrides it, which is
// how the tests run against a dictionary of their own.
QStringList searchPaths();
} // namespace Spelling
