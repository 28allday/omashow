#pragma once

// Everything a deck can be turned into, as one description and one runner.
//
// Pictures and film come out of the same evaluator the canvas and the show use,
// at times taken from the deck's own clock — so an exported frame is the frame
// that was rehearsed, not a second rendering of roughly the same thing.
// Encoding shells out to ffmpeg, as media optimisation already does.

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <functional>

#include "core/scene.h"
#include "core/workers.h"

namespace Exports {

enum Kind { Pdf = 0, Images = 1, Video = 2, Package = 3, Print = 4, PowerPoint = 5 };

struct Request {
    int kind = Pdf;
    QString path;             // the file to write, or the first of a numbered set
    bool includeSkipped = false;
    bool stages = false;      // PDF: a page for every build stage
    int layout = 0;           // PDF and print: slides, notes, outline, handout
    int perPage = 2;          // handout sheets
    int from = -1, to = -1;   // slide range, -1 for the whole deck
    // Pictures
    int format = 0;           // 0 PNG, 1 JPEG
    int width = 1920;
    bool transparent = false; // PNG only: leave the slide background out
    // Film
    int fps = 30;
    int quality = 0;          // 0 balanced, 1 high
    // Printing
    QString printer;
    int copies = 1;

    QVariantMap toMap() const;
    static Request fromMap(const QVariantMap &map);
    QString describe() const;
    QString suggestedName(const QString &deckName) const;
};

struct Outcome {
    bool ok = false;
    QString error;
    QStringList files;
    QStringList log;
};

// `progress` is called with 0–100 from the worker thread. `approved` carries
// the linked files the author has allowed to be read, for packaging.
Outcome run(const Document &document, const Request &request,
            const std::shared_ptr<Workers::Job> &job,
            const std::function<void(int)> &progress = {},
            const QHash<QString, QString> &approved = {});

// Every file `run` would write for this request (pictures are one per slide),
// so a caller can refuse to replace any that already exist.
QStringList targets(const Document &document, const Request &request);

// Whether ffmpeg is here at all, so film can be offered honestly.
bool encoderAvailable();
} // namespace Exports
