#include "core/link.h"
#include <QRegularExpression>
namespace {
bool controls(const QString &text) {
  for (const auto c : text)
    if (c.unicode() < 32 || c.unicode() == 127)
      return true;
  return false;
}
} // namespace
QString Links::normalized(int kind, const QString &target) {
  auto text = target.trimmed();
  if (kind == 2 && !text.startsWith("mailto:", Qt::CaseInsensitive))
    text = "mailto:" + text;
  if (kind == 1 || kind == 2)
    return QUrl(text, QUrl::StrictMode).toString(QUrl::FullyEncoded);
  return kind == 3 ? text : QString();
}
QString Links::validate(int kind, const QString &target,
                        const Document &document, bool allowMissingSlide) {
  if (kind < 0 || kind > 8)
    return "This action is unsupported.";
  if (kind == 0 || kind >= 4)
    return {};
  if (target.size() > 8192 || controls(target))
    return "The link contains invalid characters or is too long.";
  if (kind == 3) {
    for (const auto &slide : document.slides)
      if (slide.id == target.trimmed())
        return {};
    return allowMissingSlide ? QString()
                             : QString("The linked slide no longer exists.");
  }
  const QUrl url(normalized(kind, target), QUrl::StrictMode);
  if (!url.isValid() || controls(url.path(QUrl::FullyDecoded)) ||
      controls(url.query(QUrl::FullyDecoded)))
    return "Enter a valid link.";
  if (kind == 1 && ((url.scheme() != "https" && url.scheme() != "http") ||
                    url.host().isEmpty() || !url.userInfo().isEmpty()))
    return "Web links must use http or https and must not contain login "
           "credentials.";
  if (kind == 2) {
    static const QRegularExpression address(
        "^[^\\s<>@,;]+@[^\\s<>@,;]+\\.[^\\s<>@,;]+$");
    if (url.scheme() != "mailto" || !url.host().isEmpty() ||
        !address.match(url.path()).hasMatch())
      return "Enter one valid email address.";
  }
  return {};
}
QString Links::issue(const SceneObject &object, const Document &document) {
  return validate(object.linkKind, object.linkTarget, document);
}
