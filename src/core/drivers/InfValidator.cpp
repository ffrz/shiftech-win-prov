#include "InfValidator.h"

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace shiftech::core::drivers {

namespace {

// Value for `key` in any section, first match, case-insensitive key.
QString iniValue(const QString& text, const QString& key) {
    const QRegularExpression re(
        "^\\s*" + QRegularExpression::escape(key) + "\\s*=\\s*(.+?)\\s*$",
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    const auto m = re.match(text);
    if (!m.hasMatch()) return {};
    QString v = m.captured(1);
    // strip surrounding quotes and trailing comments
    const int semi = v.indexOf(';');
    if (semi >= 0) v = v.left(semi).trimmed();
    if (v.startsWith('"') && v.endsWith('"') && v.size() >= 2) v = v.mid(1, v.size() - 2);
    return v.trimmed();
}

bool hasSection(const QString& text, const QString& name) {
    const QRegularExpression re("^\\s*\\[\\s*" + QRegularExpression::escape(name) + "\\s*\\]",
                                QRegularExpression::CaseInsensitiveOption |
                                    QRegularExpression::MultilineOption);
    return re.match(text).hasMatch();
}

} // namespace

InfValidation validateInfText(const std::string& infTextStd) {
    InfValidation v;
    const QString text = QString::fromStdString(infTextStd);

    if (text.trimmed().isEmpty()) {
        v.messages.push_back("empty file");
        v.verdict = InfVerdict::Reject;
        return v;
    }

    if (!hasSection(text, "Version")) {
        v.messages.push_back("no [Version] section");
        v.verdict = InfVerdict::Reject;
        return v;
    }

    const QString sig = iniValue(text, "Signature");
    if (sig.isEmpty()) {
        v.messages.push_back("no Signature= in [Version]");
        v.verdict = InfVerdict::Reject;
        return v;
    }

    v.className = iniValue(text, "Class").toStdString();
    v.classGuid = iniValue(text, "ClassGuid").toStdString();
    if (v.className.empty() && v.classGuid.empty()) {
        v.messages.push_back("no Class / ClassGuid");
        v.verdict = InfVerdict::Reject;
        return v;
    }

    const QString cat = iniValue(text, "CatalogFile");
    v.hasCatalog = !cat.isEmpty();

    InfVerdict worst = InfVerdict::Ok;
    if (!v.hasCatalog) {
        v.messages.push_back("no CatalogFile (package is unsigned)");
        worst = InfVerdict::Warn;
    }

    // Flag a few directives that have no place in a plain driver INF. Warn, don't reject,
    // unless clearly hostile.
    static const QStringList suspicious = {"RunOnce", "AddService.*cmd", "\\.exe\\b"};
    for (const QString& pat : suspicious) {
        const QRegularExpression re(pat, QRegularExpression::CaseInsensitiveOption);
        if (re.match(text).hasMatch()) {
            v.messages.push_back("suspicious content matched /" + pat.toStdString() + "/");
            if (worst == InfVerdict::Ok) worst = InfVerdict::Warn;
        }
    }

    v.verdict = worst;
    return v;
}

InfValidation validateInf(const std::string& infFilePath) {
    QFile f(QString::fromStdString(infFilePath));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        InfValidation v;
        v.verdict = InfVerdict::Reject;
        v.messages.push_back("cannot open " + infFilePath);
        return v;
    }
    return validateInfText(QString::fromUtf8(f.readAll()).toStdString());
}

} // namespace shiftech::core::drivers
