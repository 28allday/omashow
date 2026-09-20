#pragma once

// Everything a deck can be turned into, as one description and one runner.
//
// Pictures and film come out of the same evaluator the canvas and the show use,
// at times taken from the deck's own clock — so an exported frame is the frame
// that was rehearsed, not a second rendering of roughly the same thing.
// Encoding shells out to ffmpeg, as media optimisation already does.

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <functional>

#include "core/scene.h"
#include "core/workers.h"

namespace Exports {

enum Kind { Pdf = 0, Images = 1, Video = 2 };

struct Request {
    int kind = Pdf;
    QString path;             // the file to write, or the first of a numbered set
    bool includeSkipped = false;
    bool stages = false;      // PDF: a page for every build stage
    int from = -1, to = -1;   // slide range, -1 for the whole deck
    // Pictures
    int format = 0;           // 0 PNG, 1 JPEG
    int width = 1920;
    bool transparent = false; // PNG only: leave the slide background out
    // Film
    int fps = 30;
    int quality = 0;          // 0 balanced, 1 high

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

// `progress` is called with 0–100 from the worker thread.
Outcome run(const Document &document, const Request &request,
            const std::shared_ptr<Workers::Job> &job,
            const std::function<void(int)> &progress = {});

// Whether ffmpeg is here at all, so film can be offered honestly.
bool encoderAvailable();
} // namespace Exports
