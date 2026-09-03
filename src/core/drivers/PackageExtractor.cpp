#include "PackageExtractor.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>

namespace shiftech::core::drivers {

namespace {

// Run a console tool, capture combined output, enforce a timeout.
struct ProcOut {
    int exitCode = -1;
    bool timedOut = false;
    QString output;
};

ProcOut run(const QString& program, const QStringList& args, const QString& workDir,
            int timeoutMs) {
    ProcOut o;
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    if (!workDir.isEmpty()) p.setWorkingDirectory(workDir);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start();
    if (!p.waitForStarted(10000)) {
        o.output = "failed to start " + program;
        return o;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(3000);
        o.timedOut = true;
        o.output = program + " timed out";
        return o;
    }
    o.exitCode = p.exitCode();
    o.output = QString::fromLocal8Bit(p.readAll());
    return o;
}

void copyDirRecursive(const QString& from, const QString& to) {
    QDir().mkpath(to);
    QDirIterator it(from, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString rel = QDir(from).relativeFilePath(it.filePath());
        const QString dst = QDir(to).filePath(rel);
        if (it.fileInfo().isDir()) {
            QDir().mkpath(dst);
        } else {
            QDir().mkpath(QFileInfo(dst).path());
            QFile::remove(dst);
            QFile::copy(it.filePath(), dst);
        }
    }
}

} // namespace

ExtractResult PackageExtractor::extract(const QString& payloadPath, const QString& destDir,
                                        int timeoutMs) {
    ExtractResult r;
    const QFileInfo fi(payloadPath);
    if (!fi.exists()) {
        r.error = "payload not found: " + payloadPath.toStdString();
        return r;
    }
    QDir().mkpath(destDir);

    // Directory or bare .inf: use in place.
    if (fi.isDir()) {
        copyDirRecursive(payloadPath, destDir);
        r.ok = true;
        r.extractedDir = destDir;
        return r;
    }
    const QString suffix = fi.suffix().toLower();
    if (suffix == "inf") {
        const QString dst = QDir(destDir).filePath(fi.fileName());
        QFile::remove(dst);
        if (!QFile::copy(payloadPath, dst)) {
            r.error = "cannot copy inf into " + destDir.toStdString();
            return r;
        }
        r.ok = true;
        r.extractedDir = destDir;
        return r;
    }

    if (suffix == "zip") {
        // Prefer bsdtar (handles zip on Win10 1803+).
        ProcOut o = run("tar", {"-xf", QDir::toNativeSeparators(payloadPath), "-C",
                                QDir::toNativeSeparators(destDir)},
                        {}, timeoutMs);
        if (o.timedOut) { r.error = "tar timed out"; return r; }
        if (o.exitCode == 0) {
            r.ok = true;
            r.extractedDir = destDir;
            return r;
        }
        // Fallback: PowerShell Expand-Archive.
        ProcOut ps = run("powershell",
                         {"-NoProfile", "-NonInteractive", "-Command",
                          QString("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
                              .arg(payloadPath, destDir)},
                         {}, timeoutMs);
        if (!ps.timedOut && ps.exitCode == 0) {
            r.ok = true;
            r.extractedDir = destDir;
            return r;
        }
        r.error = "zip extraction failed: " + o.output.toStdString();
        return r;
    }

    if (suffix == "cab") {
        ProcOut o = run("expand",
                        {QDir::toNativeSeparators(payloadPath), "-F:*",
                         QDir::toNativeSeparators(destDir)},
                        {}, timeoutMs);
        if (o.timedOut) { r.error = "expand timed out"; return r; }
        if (o.exitCode == 0) {
            r.ok = true;
            r.extractedDir = destDir;
            return r;
        }
        r.error = "cab extraction failed: " + o.output.toStdString();
        return r;
    }

    r.error = "unsupported package type: ." + suffix.toStdString();
    return r;
}

std::vector<std::string> PackageExtractor::findInfFiles(const QString& dir) {
    std::vector<std::string> out;
    QDirIterator it(dir, {"*.inf"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        out.push_back(it.next().toStdString());
    }
    return out;
}

} // namespace shiftech::core::drivers
