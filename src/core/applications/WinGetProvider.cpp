#include "WinGetProvider.h"
#include "WinGetOutput.h"
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QProcess>
#include <QThread>

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

WinGetBootstrapResult WinGetProvider::bootstrap(const QString& bundleDir) {
    WinGetBootstrapResult r;

    if (m_available) {
        r.ok = true;
        r.alreadyPresent = true;
        r.detail = "winget already available";
        return r;
    }

    // Resolve the bundle dir: given, else <exeDir>/tools/winget (+ repo fallbacks).
    QString dir = bundleDir;
    if (dir.isEmpty()) {
        const QString exe = QCoreApplication::applicationDirPath();
        for (const QString& c : {exe + "/tools/winget", exe + "/../tools/winget",
                                 exe + "/../../tools/winget"}) {
            if (QDir(c).exists()) { dir = c; break; }
        }
    }
    if (dir.isEmpty() || !QDir(dir).exists()) {
        r.detail = "no bundled App Installer found (tools/winget/)";
        return r;
    }

    QStringList pkgs;
    for (const QFileInfo& fi :
         QDir(dir).entryInfoList({"*.msixbundle", "*.appxbundle", "*.msix", "*.appx"},
                                 QDir::Files, QDir::Name)) {
        pkgs << fi.absoluteFilePath();
    }
    if (pkgs.isEmpty()) {
        r.detail = "tools/winget/ has no *.msixbundle / *.appx packages";
        return r;
    }

    // Install every package. Dependencies (VCLibs, UI.Xaml) must sort before the main
    // bundle, or be passed via -DependencyPath; sorting by name puts Microsoft.VCLibs*
    // and Microsoft.UI.Xaml* before Microsoft.DesktopAppInstaller*. Add-AppxPackage is
    // idempotent, so we just install each in order and let a later one satisfy an earlier
    // dependency error on a retry pass.
    QString log;
    for (int pass = 0; pass < 2; ++pass) {
        for (const QString& p : pkgs) {
            QProcess ps;
            ps.setProgram("powershell");
            ps.setArguments({"-NoProfile", "-NonInteractive", "-Command",
                             QString("Add-AppxPackage -Path '%1' -ForceApplicationShutdown")
                                 .arg(p)});
            ps.setProcessChannelMode(QProcess::MergedChannels);
            ps.start();
            ps.waitForFinished(180000);
            log += QFileInfo(p).fileName() + " -> exit " +
                   QString::number(ps.exitCode()) + "\n";
        }
        m_available = checkAvailability();
        if (m_available) break;
    }

    r.ok = m_available;
    r.detail = m_available ? ("installed App Installer from " + dir.toStdString())
                           : ("Add-AppxPackage did not enable winget:\n" + log.toStdString());
    return r;
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
