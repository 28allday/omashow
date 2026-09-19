#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

// The XDG desktop portal file chooser. Under Wayland this is the only picker
// that looks and behaves like the rest of the desktop — never QFileDialog.
//
// Both calls are asynchronous: they return immediately and the answer arrives as
// `selected` or `canceled`.
class PortalFileChooser : public QObject {
    Q_OBJECT

public:
    explicit PortalFileChooser(QObject *parent = nullptr);

    // `patterns` are globs, e.g. {"*.md", "*.markdown"}; empty means any file.
    Q_INVOKABLE void openFile(const QString &title, const QString &filterName = QString(),
                              const QStringList &patterns = {});
    Q_INVOKABLE void saveFile(const QString &title, const QString &suggestedName,
                              const QString &filterName = QString(),
                              const QStringList &patterns = {});

signals:
    void selected(const QUrl &url);
    void canceled();
    void failed(const QString &message);

private slots:
    void handleResponse(uint response, const QVariantMap &results);

private:
    bool request(const QString &method, const QString &title, QVariantMap options);
    void clearPending();

    QString m_pendingPath;
};
