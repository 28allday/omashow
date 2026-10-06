QT += core gui qml quick quickcontrols2 dbus

CONFIG += c++17 release
TARGET = omashow
TEMPLATE = app

INCLUDEPATH += src

HEADERS += \
    src/backend.h \
    src/filepicker.h \
    src/fixture.h \
    src/showguard.h \
    src/presenter.h \
    src/slideview.h \
    src/thumbnailprovider.h \
    src/sliderows.h \
    src/theme.h \
    src/anim/build.h \
    src/anim/evaluator.h \
    src/anim/morph.h \
    src/anim/presentation.h \
    src/core/arrange.h \
    src/core/design.h \
    src/core/edit.h \
    src/core/history.h \
    src/core/scene.h \
    src/core/snap.h \
    src/io/bundle.h \
    src/io/pdf.h \
    src/io/recovery.h \
    src/io/zip.h \
    src/render/scenerenderer.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/backenddesign.cpp \
    src/backendanimation.cpp \
    src/backendselection.cpp \
    src/filepicker.cpp \
    src/fixture.cpp \
    src/showguard.cpp \
    src/presenter.cpp \
    src/slideview.cpp \
    src/thumbnailprovider.cpp \
    src/sliderows.cpp \
    src/theme.cpp \
    src/anim/build.cpp \
    src/anim/evaluator.cpp \
    src/anim/morph.cpp \
    src/anim/presentation.cpp \
    src/core/arrange.cpp \
    src/core/design.cpp \
    src/core/edit.cpp \
    src/core/history.cpp \
    src/core/scene.cpp \
    src/core/snap.cpp \
    src/io/bundle.cpp \
    src/io/pdf.cpp \
    src/io/recovery.cpp \
    src/io/zip.cpp \
    src/render/scenerenderer.cpp

RESOURCES += src/resources.qrc

# zlib is already underneath Qt; the bundle writer uses it directly.
LIBS += -lz

HEADERS += src/core/starter.h src/io/recents.h
SOURCES += src/core/starter.cpp src/io/recents.cpp src/backendstart.cpp

HEADERS += src/core/objectcopy.h
SOURCES += src/core/objectcopy.cpp src/backendclipboard.cpp

HEADERS += src/render/textlayout.h
SOURCES += src/render/textlayout.cpp
HEADERS += src/render/mathlayout.h
SOURCES += src/render/mathlayout.cpp

HEADERS += src/core/imageasset.h
SOURCES += src/core/imageasset.cpp src/backendimage.cpp

HEADERS += src/core/slides.h
SOURCES += src/core/slides.cpp src/backendslides.cpp

HEADERS += src/core/deckresize.h
SOURCES += src/core/deckresize.cpp

QT += svg
HEADERS += src/core/svgasset.h
SOURCES += src/core/svgasset.cpp

HEADERS += src/core/shape.h
SOURCES += src/core/shape.cpp src/backendshape.cpp

HEADERS += src/core/objectstyle.h
SOURCES += src/core/objectstyle.cpp

HEADERS += src/core/connector.h
SOURCES += src/core/connector.cpp src/backendconnector.cpp

HEADERS += src/core/link.h
SOURCES += src/core/link.cpp src/backendlink.cpp

HEADERS += src/core/imagecrop.h
SOURCES += src/core/imagecrop.cpp

QT += multimedia concurrent printsupport
CONFIG += link_pkgconfig
PKGCONFIG += libavformat libavcodec libavutil libswscale
PKGCONFIG += hunspell
HEADERS += src/core/spelling.h
SOURCES += src/core/spelling.cpp
HEADERS += src/core/punctuation.h
SOURCES += src/core/punctuation.cpp
HEADERS += src/cli/operations.h
SOURCES += src/cli/operations.cpp
HEADERS += src/cli/describe.h
SOURCES += src/cli/describe.cpp
HEADERS += src/cli/cli.h
SOURCES += src/cli/cli.cpp
HEADERS += src/core/mediaasset.h
SOURCES += src/core/mediaasset.cpp

SOURCES += src/backendmedia.cpp

HEADERS += src/mediaplayback.h
SOURCES += src/mediaplayback.cpp

HEADERS += src/core/mediatranscode.h
SOURCES += src/core/mediatranscode.cpp

SOURCES += src/backendmediaoptimisation.cpp

HEADERS += src/core/imageoptimisation.h
SOURCES += src/core/imageoptimisation.cpp

HEADERS += src/core/tabledata.h src/core/table.h
SOURCES += src/core/table.cpp

HEADERS += src/tablemodel.h src/core/delimited.h
SOURCES += src/tablemodel.cpp src/backendtable.cpp src/core/delimited.cpp

HEADERS += src/tableaccessibility.h
SOURCES += src/tableaccessibility.cpp

HEADERS += src/core/chartdata.h src/core/chart.h
SOURCES += src/core/chart.cpp

SOURCES += src/backendchart.cpp

HEADERS += src/core/datasource.h
SOURCES += src/core/datasource.cpp

HEADERS += src/core/diagram.h
SOURCES += src/core/diagram.cpp src/backenddiagram.cpp

HEADERS += src/core/layoutapply.h
SOURCES += src/core/layoutapply.cpp src/backendlayout.cpp
HEADERS += src/core/deckimport.h src/core/deckaudit.h
SOURCES += src/core/deckimport.cpp src/core/deckaudit.cpp src/backenddesignimport.cpp
HEADERS += src/core/review.h src/core/findreplace.h
SOURCES += src/core/review.cpp src/core/findreplace.cpp src/backendreview.cpp
HEADERS += src/core/textruns.h
SOURCES += src/core/textruns.cpp
HEADERS += src/core/templates.h
SOURCES += src/core/templates.cpp src/backendtemplates.cpp
HEADERS += src/io/exports.h src/io/printing.h src/io/packagedeck.h src/io/decklock.h
SOURCES += src/io/exports.cpp src/io/printing.cpp src/io/packagedeck.cpp src/io/decklock.cpp src/backendexports.cpp
SOURCES += src/core/designfields.cpp

HEADERS += src/anim/presentationcache.h src/render/liveframes.h
SOURCES += src/anim/presentationcache.cpp src/render/liveframes.cpp
HEADERS += src/core/workers.h
SOURCES += src/core/workers.cpp src/backendjobs.cpp
HEADERS += src/core/hardwaredecode.h
SOURCES += src/core/hardwaredecode.cpp

# Decks from elsewhere: PowerPoint read in-house, Keynote through libetonyek.
PKGCONFIG += libetonyek-0.1 librevenge-0.0 librevenge-stream-0.0
HEADERS += src/io/interchange.h src/io/pptx.h src/io/keynote.h src/io/pptxwriter.h
SOURCES += src/io/interchange.cpp src/io/pptx.cpp src/io/keynote.cpp src/io/pptxwriter.cpp
HEADERS += src/core/fonts.h
SOURCES += src/core/fonts.cpp
