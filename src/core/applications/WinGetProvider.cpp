#include "WinGetProvider.h"
#include <QProcess>
#include <QDebug>

namespace shiftech::core::applications {

WinGetProvider::WinGetProvider() {
    m_available = checkAvailability();
}

bool WinGetProvider::checkAvailability() {
    QStringList args;
    args << "--version";
    ProcessResult res = runWinGet(args, 10000);
    return !res.timedOut && res.exitCode == 0;
}

WinGetProvider::ProcessResult WinGetProvider::runWinGet(const QStringList& args, int timeoutMs) {
    QProcess process;
    process.setProgram("winget");
    process.setArguments(args);
    
    ProcessResult result;
    result.exitCode = -1;
    result.timedOut = false;

    process.start();
    if (!process.waitForStarted(5000)) {
        result.stdErr = "Failed to start winget process.";
        return result;
    }

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished();
        result.timedOut = true;
        result.stdErr = "Process timed out.";
        return result;
    }

    result.exitCode = process.exitCode();
    result.stdOut = process.readAllStandardOutput().toStdString();
    result.stdErr = process.readAllStandardError().toStdString();
    
    return result;
}

bool WinGetProvider::isInstalled(const std::string& id) {
    if (!m_available) return false;

    QStringList args;
    args << "list" << "--id" << QString::fromStdString(id) << "--exact";

    ProcessResult res = runWinGet(args, 30000);

    // winget exits with non-zero or prints "No installed package found matching input criteria."
    if (res.exitCode != 0) {
        return false;
    }

    QString outStr = QString::fromStdString(res.stdOut);
    if (outStr.contains("No installed package found", Qt::CaseInsensitive)) {
        return false;
    }

    // If it prints the table, the package is installed
    return true;
}

InstallResult WinGetProvider::install(const std::string& id, const InstallOptions&) {
    InstallResult result;
    
    if (!m_available) {
        result.ok = false;
        result.log = "WinGet is not available on this system.";
        return result;
    }

    if (isInstalled(id)) {
        result.ok = true;
        result.alreadyInstalled = true;
        result.log = "Package already installed.";
        return result;
    }

    QStringList args;
    args << "install" << "--id" << QString::fromStdString(id) 
         << "--exact" << "--silent" 
         << "--accept-package-agreements" 
         << "--accept-source-agreements" 
         << "--disable-interactivity";

    // Retry once on transient failures
    for (int attempt = 1; attempt <= 2; ++attempt) {
        ProcessResult res = runWinGet(args, 600000); // 10 mins

        result.exitCode = res.exitCode;
        result.log = res.stdOut + "\n" + res.stdErr;

        if (res.timedOut) {
            result.ok = false;
            result.log += "\nInstallation timed out.";
            break; // don't retry on timeout
        }

        if (res.exitCode == 0) {
            result.ok = true;
            break;
        }

        // Common transient network/source codes (very broad guess for winget, e.g. 0x801901a0, etc)
        // If it's the first attempt and failed, we retry. If it's the second attempt, we just stop.
        if (attempt == 1) {
            qDebug() << "WinGet install failed for" << id.c_str() << "with code" << res.exitCode << "- retrying...";
            // QThread::msleep(2000); // Could wait if we included QThread
        } else {
            result.ok = false;
        }
    }

    return result;
}

} // namespace shiftech::core::applications
