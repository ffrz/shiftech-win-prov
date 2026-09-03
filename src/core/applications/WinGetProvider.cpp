#include "WinGetProvider.h"
#include "WinGetOutput.h"
#include <QProcess>
#include <QThread>
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
    if (res.timedOut) {
        return false;
    }
    return winget::listOutputSaysInstalled(res.exitCode, res.stdOut);
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

        result.ok = false;
        if (attempt == 1 && winget::isTransientInstallFailure(res.exitCode)) {
            qDebug() << "WinGet install failed for" << id.c_str()
                     << "with code" << res.exitCode << "- transient, retrying in 3s...";
            QThread::msleep(3000);
        } else {
            break; // permanent failure or out of retries
        }
    }

    return result;
}

} // namespace shiftech::core::applications
