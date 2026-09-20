#include "backend.h"
#include "core/templates.h"
#include "filepicker.h"
#include "io/recovery.h"
#include <QFileInfo>

QVariantList Backend::templates() const { return Templates::all(); }

void Backend::refreshTemplates() { emit templatesChanged(); }

bool Backend::createFromTemplate(const QString &id) {
    Document document;
    QString error;
    if (!Templates::open(id, &document, &error)) {
        setStatus(error);
        emit failed(error);
        return false;
    }
    m_autosave.stop();
    Recovery::discard();
    setDocument(document);
    // A template makes a new deck: it has no file behind it, so the first save
    // asks where it should go and the template itself is never overwritten.
    setFileUrl(QUrl());
    QVariantMap row;
    Templates::describe(id, &row);
    setStatus(row.isEmpty() ? tr("New deck")
                            : tr("New deck from %1").arg(row.value("name").toString()));
    return true;
}

void Backend::installTemplateDialog() {
    m_pending = Pending::InstallTemplate;
    m_chooser->openFile(tr("Install a template"), tr("OmaShow decks"),
                        {QStringLiteral("*.omashow")});
}

bool Backend::installTemplate(const QUrl &url) {
    if (!url.isLocalFile()) return false;
    QString error;
    QStringList warnings;
    const auto installed = Templates::install(url.toLocalFile(), &error, &warnings);
    if (installed.isEmpty()) {
        setStatus(error);
        emit failed(error);
        return false;
    }
    setStatus(warnings.isEmpty()
                  ? tr("Installed %1").arg(QFileInfo(installed).completeBaseName())
                  : tr("Installed %1 — %2").arg(QFileInfo(installed).completeBaseName(),
                                                warnings.join(QStringLiteral(" "))));
    emit templatesChanged();
    return true;
}

bool Backend::saveAsTemplate(const QString &name) {
    QString error;
    QStringList warnings;
    const auto saved = Templates::save(m_document, name, &error, &warnings);
    if (saved.isEmpty()) {
        setStatus(error);
        emit failed(error);
        return false;
    }
    setStatus(warnings.isEmpty()
                  ? tr("Kept %1 as a template").arg(QFileInfo(saved).completeBaseName())
                  : tr("Kept %1 as a template — %2").arg(QFileInfo(saved).completeBaseName(),
                                                         warnings.join(QStringLiteral(" "))));
    emit templatesChanged();
    return true;
}

bool Backend::removeTemplate(const QString &id) {
    QString error;
    if (!Templates::remove(id, &error)) {
        setStatus(error);
        emit failed(error);
        return false;
    }
    setStatus(tr("Removed that template"));
    emit templatesChanged();
    return true;
}
