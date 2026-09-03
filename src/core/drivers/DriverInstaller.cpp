#include "DriverInstaller.h"

#include "PnpUtilOutput.h"
#include "../system/SystemInspector.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace shiftech::core::drivers {

DriverInstaller::DriverInstaller(InstallerOptions opts) : m_opts(opts) {}

InstallReport DriverInstaller::installInfs(const std::vector<std::string>& infPaths) {
    InstallReport report;
    report.ranElevated = system::SystemInspector::isElevated();

    if (!m_opts.dryRun && !report.ranElevated) {
        report.fatalError =
            "driver installation requires Administrator privileges; re-run elevated";
        return report;
    }

    for (const std::string& inf : infPaths) {
        InfInstallOutcome out;
        out.infPath = inf;

        if (!QFileInfo::exists(QString::fromStdString(inf))) {
            out.ok = false;
            out.log = "inf not found";
            report.anyFailed = true;
            report.perInf.push_back(out);
            continue;
        }

        if (m_opts.dryRun) {
            out.ok = true;
            out.log = "dry-run: would run  pnputil /add-driver \"" + inf + "\" /install";
            report.perInf.push_back(out);
            continue;
        }

        QProcess p;
        p.setProgram("pnputil");
        p.setArguments({"/add-driver", QDir::toNativeSeparators(QString::fromStdString(inf)),
                        "/install"});
        p.setProcessChannelMode(QProcess::MergedChannels);
        p.start();

        if (!p.waitForStarted(10000)) {
            out.ok = false;
            out.log = "failed to start pnputil";
            report.anyFailed = true;
            report.perInf.push_back(out);
            continue;
        }
        if (!p.waitForFinished(m_opts.perInfTimeoutMs)) {
            p.kill();
            p.waitForFinished(3000);
            out.ok = false;
            out.log = "pnputil timed out";
            report.anyFailed = true;
            report.perInf.push_back(out);
            continue;
        }

        out.exitCode = p.exitCode();
        const std::string raw = QString::fromLocal8Bit(p.readAll()).trimmed().toStdString();
        out.log = raw;

        const auto parsed = pnputil::parseAddDriver(out.exitCode, raw);
        out.ok = parsed.succeeded;
        out.rebootRequired = parsed.rebootRequired;
        out.publishedName = parsed.publishedName;

        if (out.ok) {
            report.anyInstalled = true;
            if (out.rebootRequired) report.rebootRequired = true;
        } else {
            report.anyFailed = true;
        }
        report.perInf.push_back(out);
    }

    return report;
}

} // namespace shiftech::core::drivers
