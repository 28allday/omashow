// OmaShow — native presentations for Omarchy.

#include <QGuiApplication>
#include <QFont>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "anim/presentation.h"
#include "io/bundle.h"
#include "io/pdf.h"
#include "showguard.h"
#include "presenter.h"
#include "slideview.h"
#include "tableaccessibility.h"
#include "thumbnailprovider.h"
#include "theme.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>
#include <QQmlEngine>
#include <QTextStream>
#include <QDir>
#include <QEventLoop>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSurfaceFormat>
#include <QTimer>

namespace {

// The shot harness. Every visual claim in Gate 0 is checked through this rather
// than by looking at a running window, and it is the seam the video encoder and
// the wider CLI grow out of later: one time in, one deterministic frame out.
int renderShots(const Backend &backend, const QString &outDir,
                const QString &timesSpec, int width, int fps, bool includeSkipped) {
    // An even frame sequence, named so ffmpeg can read it directly. This is the
    // video export path in embryo: the encoder's only job later is to consume
    // what the evaluator already produces at exact frame intervals.
    if (fps > 0) {
        QTextStream out(stdout);
        const qreal duration = Presentation::duration(backend.document(),includeSkipped);
        const int frames = int(duration * fps) + 1;
        for (int i = 0; i < frames; ++i) {
            const QString path = QStringLiteral("%1/frame-%2.png")
                                     .arg(outDir)
                                     .arg(i, 5, 10, QLatin1Char('0'));
            if (!backend.renderFrame(qreal(i) / fps, path, width, includeSkipped)) {
                qCritical() << "Could not write" << path;
                return 1;
            }
        }
        out << frames << " frames at " << fps << " fps" << Qt::endl;
        return 0;
    }

    QStringList times = timesSpec.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (times.isEmpty()) {
        // A spread that lands inside every build and across the transition.
        const qreal duration = Presentation::duration(backend.document(),includeSkipped);
        for (int i = 0; i <= 12; ++i)
            times << QString::number(duration * i / 12.0, 'f', 3);
    }

    QTextStream out(stdout);
    int index = 0;
    for (const QString &spec : times) {
        bool ok = false;
        const qreal t = spec.trimmed().toDouble(&ok);
        if (!ok) {
            qCritical() << "Not a time:" << spec;
            return 2;
        }
        const QString path = QStringLiteral("%1/frame-%2-t%3.png")
                                 .arg(outDir)
                                 .arg(index, 3, 10, QLatin1Char('0'))
                                 .arg(t, 0, 'f', 3);
        if (!backend.renderFrame(t, path, width, includeSkipped)) {
            qCritical() << "Could not write" << path;
            return 1;
        }
        out << path << Qt::endl;
        ++index;
    }
    return 0;
}

} // namespace

// Photograph the interface, one PNG per workspace, for design review off the
// user's screen. Run offscreen: QT_QPA_PLATFORM=offscreen omashow --ui-shot dir
int captureInterface(QQmlApplicationEngine &engine, Backend &backend, Presenter &presenter,
                     const QString &outDir, const QSize &size, bool haveDeck) {
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window) return 1;
    QDir().mkpath(outDir);
    window->resize(size);
    const auto settle = [](int ms) { QEventLoop loop; QTimer::singleShot(ms, &loop, &QEventLoop::quit); loop.exec(); };
    const auto save = [&](QQuickWindow *w, const QString &name) {
        settle(300);
        w->grabWindow().save(outDir + QLatin1Char('/') + name + QStringLiteral(".png"));
    };
    if (!haveDeck) {
        save(window, QStringLiteral("start"));
        backend.createDeck(0, 1920, 1080, 0);
    }
    const struct { int workspace; const char *name; } views[] = {
        {0, "edit"}, {1, "design"}, {2, "animate"}, {4, "present"}, {5, "export"}, {6, "sorter"}};
    for (const auto &view : views) {
        window->setProperty("workspace", view.workspace);
        save(window, QString::fromLatin1(view.name));
    }
    window->setProperty("workspace", 0);
    const QVariantList objects = backend.slideObjects();
    if (!objects.isEmpty()) {
        backend.select(objects.first().toMap().value(QStringLiteral("id")).toString());
        save(window, QStringLiteral("edit-selected"));
        backend.setProperty("currentSlide", 0);
    }
    // The presenter console, rehearsing so no display has to exist.
    presenter.start(false, true);
    for (QObject *root : engine.rootObjects())
        for (auto *console : root->findChildren<QQuickWindow *>(QStringLiteral("presenterConsole"))) {
            console->resize(size);
            save(console, QStringLiteral("presenter-console"));
        }
    presenter.stop();
    return 0;
}

int main(int argc, char *argv[]) {
    // FBO-backed QPainter is supported on Qt's OpenGL renderer. Honour explicit
    // platform/backend choices, including the software screenshot harness.
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND") &&
        qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND") && qgetenv("OMASHOW_RENDERER") != "raster") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
        auto format = QSurfaceFormat::defaultFormat();
        format.setSamples(4);
        QSurfaceFormat::setDefaultFormat(format);
    }
    QGuiApplication app(argc, argv);
    Workers::Session workerSession;

    // The identity trio. setDesktopFileName is what becomes the Wayland app_id,
    // which is what Hyprland rules, the taskbar and the icon lookup key off.
    app.setApplicationName(QStringLiteral("omashow"));
    app.setDesktopFileName(QStringLiteral("omashow"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("omashow")));

    // OmaShow's own Controls style (src/style/OmaShowStyle, in qrc under the
    // default qt/qml import path): the Oma suite look, falling back to Basic
    // for anything it does not restyle.
    QQuickStyle::setStyle(QStringLiteral("OmaShowStyle"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("OmaShow — presentations for Omarchy."));
    parser.addHelpOption();
    QCommandLineOption shotOption(QStringLiteral("shot"),
                                  QStringLiteral("Render frames to <dir> and exit."),
                                  QStringLiteral("dir"));
    QCommandLineOption timesOption(QStringLiteral("times"),
                                   QStringLiteral("Comma-separated times in seconds."),
                                   QStringLiteral("list"));
    QCommandLineOption widthOption(QStringLiteral("width"),
                                   QStringLiteral("Frame width in pixels (default 1920)."),
                                   QStringLiteral("px"), QStringLiteral("1920"));
    // Open parked at a given second. Handy for looking at one moment of a build
    // without scrubbing to it by hand, and it is what the headless GUI check
    // uses to photograph the live view mid-transition.
    QCommandLineOption atOption(QStringLiteral("at"),
                                QStringLiteral("Open at <seconds> instead of the start."),
                                QStringLiteral("seconds"));
    parser.addOption(shotOption);
    parser.addOption(timesOption);
    parser.addOption(widthOption);
    // The start of the brief's `omashow new`: write a deck and exit. It is also
    // how the container gets checked against an unzip that is not ours.
    QCommandLineOption writeOption(QStringLiteral("write"),
                                   QStringLiteral("Write the current deck to <file> and exit."),
                                   QStringLiteral("file"));
    parser.addOption(writeOption);
    QCommandLineOption pdfOption(QStringLiteral("pdf"),
                                 QStringLiteral("Export the deck to <file> and exit."),
                                 QStringLiteral("file"));
    QCommandLineOption stagesOption(QStringLiteral("stages"),
                                    QStringLiteral("One PDF page per build stage."));
    parser.addOption(pdfOption);
    parser.addOption(stagesOption);
    QCommandLineOption skippedOption(QStringLiteral("include-skipped"),QStringLiteral("Include skipped slides in PNG and PDF exports."));
    parser.addOption(skippedOption);
    QCommandLineOption fpsOption(QStringLiteral("fps"),
                                 QStringLiteral("Render an even sequence at <n> frames a second."),
                                 QStringLiteral("n"), QStringLiteral("0"));
    parser.addOption(atOption);
    parser.addOption(fpsOption);
    QCommandLineOption uiShotOption(QStringLiteral("ui-shot"),
                                    QStringLiteral("Photograph each workspace to <dir> and exit."),
                                    QStringLiteral("dir"));
    QCommandLineOption uiSizeOption(QStringLiteral("ui-size"),
                                    QStringLiteral("Window size for --ui-shot (default 1920x1080)."),
                                    QStringLiteral("WxH"), QStringLiteral("1920x1080"));
    parser.addOption(uiShotOption);
    parser.addOption(uiSizeOption);
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Deck to open."));
    parser.process(app);

    OmarchyTheme theme;
    Backend backend;
    ShowGuard showGuard;
    Presenter presenter(&backend, &showGuard);

    // Headless commands must load their positional deck before exporting.
    const auto cliFiles = parser.positionalArguments();
    if (!cliFiles.isEmpty() && (parser.isSet(writeOption) || parser.isSet(pdfOption) || parser.isSet(shotOption))) {
        const auto loaded = Bundle::load(cliFiles.first());
        if (!loaded.ok) { qCritical().noquote() << loaded.error; return 1; }
        backend.setDocument(loaded.document);
    }

    if (parser.isSet(writeOption)) {
        const QString path = parser.value(writeOption);
        QString error;
        if (!Bundle::save(backend.document(), path, &error)) {
            qCritical().noquote() << error;
            return 1;
        }
        QTextStream(stdout) << path << Qt::endl;
        return 0;
    }

    if (parser.isSet(pdfOption)) {
        Pdf::Options options;
        options.pagePerBuildStage = parser.isSet(stagesOption);
        options.includeSkipped = parser.isSet(skippedOption);
        QString error;
        if (!Pdf::write(backend.document(), parser.value(pdfOption), options, &error)) {
            qCritical().noquote() << error;
            return 1;
        }
        QTextStream(stdout) << parser.value(pdfOption) << Qt::endl;
        return 0;
    }

    if (parser.isSet(shotOption)) {
        return renderShots(backend, parser.value(shotOption), parser.value(timesOption),
                           parser.value(widthOption).toInt(), parser.value(fpsOption).toInt(), parser.isSet(skippedOption));
    }

    // A capture run rehearses a show, and a rehearsal must never reach the
    // real desktop: no do-not-disturb, no idle inhibitor.
    if (parser.isSet(uiShotOption)) {
        showGuard.setIdleInhibitor(QString());
        showGuard.setRunner([](const QString &, const QStringList &) { return QString(); });
    }

    if (parser.isSet(atOption)) {
        backend.setDocument(backend.document());
        backend.setTime(parser.value(atOption).toDouble());
    }

        qmlRegisterType<TableAccessibility>("Omashow",1,0,"TableAccessibility");
    qmlRegisterType<SlideView>("Omashow", 1, 0, "SlideView");
    // The desktop palette, and the design system built on top of it. Theme.qml
    // is the single source of every colour and dimension in the interface; see
    // the note at the top of it.
    qmlRegisterSingletonInstance("Omashow", 1, 0, "OmarchyTheme", &theme);
    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/Theme.qml")),
                             "Omashow", 1, 0, "Theme");
    // Lucide line icons, tinted without a shader (see src/ui/Icon.qml).
    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/ui/icons/Icons.qml")),
                             "Omashow", 1, 0, "Icons");
    qmlRegisterType(QUrl(QStringLiteral("qrc:/ui/Icon.qml")), "Omashow", 1, 0, "Icon");
    // How many sheets are in front of the deck, so the shell's shortcuts can
    // stand down while one has the keyboard (see src/Sheet.qml).
    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/ui/Sheets.qml")),
                             "Omashow", 1, 0, "Sheets");

    // Carry the desktop's text size into the default font so every control
    // grows with `omarchy display text size`, without a restart.
    const qreal basePointSize = app.font().pointSizeF();
    const auto scaleFont = [&app, basePointSize](qreal scale) {
        QFont font = app.font();
        font.setPointSizeF(basePointSize * scale);
        app.setFont(font);
    };
    scaleFont(theme.textScale());
    QObject::connect(&theme, &OmarchyTheme::textScaleChanged, &app, [&theme, scaleFont] {
        scaleFont(theme.textScale());
    });

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("slides"), new SlideThumbnailProvider(&backend));
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError> &warnings) {
        for (const QQmlError &warning : warnings)
            qWarning().noquote() << warning.toString();
    });
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("showGuard"), &showGuard);
    engine.rootContext()->setContextProperty(QStringLiteral("presenter"), &presenter);
    // Belt and braces: whatever route the app exits by, the lock comes back.
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &showGuard, &ShowGuard::release);

    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Could not load the omashow interface.";
        return -1;
    }

    // "Open with", and `omashow somefile` from a terminal.
    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        if (parser.isSet(atOption))
            QObject::connect(&backend, &Backend::opened, &app, [&] { backend.setTime(parser.value(atOption).toDouble()); }, Qt::SingleShotConnection);
        if (parser.isSet(uiShotOption)) backend.open(QUrl::fromLocalFile(positional.first()));
        else backend.openAsync(QUrl::fromLocalFile(positional.first()));
    } else if (parser.isSet(atOption)) backend.setTime(parser.value(atOption).toDouble());
    if (parser.isSet(uiShotOption)) {
        const QStringList wh = parser.value(uiSizeOption).split(QLatin1Char('x'));
        const QSize size(wh.value(0).toInt(), wh.value(1).toInt());
        return captureInterface(engine, backend, presenter, parser.value(uiShotOption),
                                size.isValid() ? size : QSize(1920, 1080), !positional.isEmpty());
    }

    return app.exec();
}
