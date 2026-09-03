#include "ConfigTweak.h"

#include <QProcess>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

namespace shiftech::core::config {

const char* toString(TweakOutcome o) {
    switch (o) {
        case TweakOutcome::Applied: return "Applied";
        case TweakOutcome::AlreadyApplied: return "AlreadyApplied";
        case TweakOutcome::Failed: return "Failed";
        case TweakOutcome::Skipped: return "Skipped";
        case TweakOutcome::RequiresReboot: return "RequiresReboot";
    }
    return "Failed";
}

namespace {

// Run a console tool, return {exitCode, combinedOutput, timedOut}.
struct Proc {
    int code = -1;
    QString out;
    bool timedOut = false;
};
Proc run(const QString& program, const QStringList& args, int timeoutMs = 30000) {
    Proc r;
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start();
    if (!p.waitForStarted(8000)) { r.out = "failed to start " + program; return r; }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill(); p.waitForFinished(3000);
        r.timedOut = true; r.out = program + " timed out"; return r;
    }
    r.code = p.exitCode();
    r.out = QString::fromLocal8Bit(p.readAll());
    return r;
}

QSettings hkcu(const QString& sub) {
    return QSettings("HKEY_CURRENT_USER\\" + sub, QSettings::NativeFormat);
}
QSettings hklm(const QString& sub) {
    return QSettings("HKEY_LOCAL_MACHINE\\" + sub, QSettings::NativeFormat);
}

const QString kExplorerAdv =
    "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";

// --- generic HKCU DWORD tweak ---------------------------------------------------
class DwordTweak : public ConfigTweak {
public:
    DwordTweak(TweakInfo i, std::function<QSettings()> reg, QString value, int wanted,
              QString restartExplorer = {})
        : m_info(std::move(i)), m_reg(std::move(reg)), m_value(std::move(value)),
          m_wanted(wanted), m_restart(std::move(restartExplorer)) {}

    TweakInfo info() const override { return m_info; }

    TweakState check() const override {
        QSettings s = m_reg();
        if (!s.contains(m_value)) return TweakState::NotApplied;
        return s.value(m_value).toInt() == m_wanted ? TweakState::Applied
                                                    : TweakState::NotApplied;
    }

    TweakResult apply(const TweakArgs&) override {
        TweakResult r{m_info.id, TweakOutcome::Failed, {}};
        if (check() == TweakState::Applied) {
            r.outcome = TweakOutcome::AlreadyApplied;
            return r;
        }
        QSettings s = m_reg();
        s.setValue(m_value, m_wanted);
        s.sync();
        if (s.status() != QSettings::NoError) {
            r.detail = "registry write failed";
            return r;
        }
        if (!m_restart.isEmpty()) {
            run("taskkill", {"/f", "/im", "explorer.exe"}, 8000);
            run("cmd", {"/c", "start", "", "explorer.exe"}, 8000);
        }
        r.outcome = TweakOutcome::Applied;
        r.detail = m_value.toStdString() + " = " + std::to_string(m_wanted);
        return r;
    }

private:
    TweakInfo m_info;
    std::function<QSettings()> m_reg;
    QString m_value;
    int m_wanted;
    QString m_restart;
};

// --- command-based tweak (net/powercfg/tzutil/...) -----------------------------
class CmdTweak : public ConfigTweak {
public:
    CmdTweak(TweakInfo i,
            std::function<TweakState()> checkFn,
            std::function<TweakResult(const TweakArgs&)> applyFn)
        : m_info(std::move(i)), m_check(std::move(checkFn)), m_apply(std::move(applyFn)) {}
    TweakInfo info() const override { return m_info; }
    TweakState check() const override { return m_check ? m_check() : TweakState::Unknown; }
    TweakResult apply(const TweakArgs& a) override { return m_apply(a); }

private:
    TweakInfo m_info;
    std::function<TweakState()> m_check;
    std::function<TweakResult(const TweakArgs&)> m_apply;
};

// --------------------------------------------------------------------------------

std::unique_ptr<ConfigTweak> makeTweak(const std::string& id) {
    auto info = [&](const char* title, const char* desc, bool elev,
                    std::vector<std::string> req = {}) {
        TweakInfo i;
        i.id = id;
        i.title = title;
        i.description = desc;
        i.needsElevation = elev;
        i.requiredArgs = std::move(req);
        return i;
    };

    if (id == "show-file-extensions")
        return std::make_unique<DwordTweak>(
            info("Show file extensions", "Explorer Advanced: HideFileExt = 0", false),
            [] { return hkcu(kExplorerAdv); }, "HideFileExt", 0, "explorer");

    if (id == "show-hidden-files")
        return std::make_unique<DwordTweak>(
            info("Show hidden files", "Explorer Advanced: Hidden = 1", false),
            [] { return hkcu(kExplorerAdv); }, "Hidden", 1, "explorer");

    if (id == "clean-taskbar-pins")
        return std::make_unique<CmdTweak>(
            info("Clean taskbar pins", "Remove pinned taskbar apps; restart Explorer", false),
            [] {
                QSettings s = hkcu("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Taskband");
                return s.contains("Favorites") ? TweakState::NotApplied : TweakState::Applied;
            },
            [](const TweakArgs&) {
                TweakResult r{"clean-taskbar-pins", TweakOutcome::Failed, {}};
                QSettings s = hkcu("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Taskband");
                s.remove("Favorites");
                s.remove("FavoritesResolve");
                s.sync();
                run("taskkill", {"/f", "/im", "explorer.exe"}, 8000);
                run("cmd", {"/c", "start", "", "explorer.exe"}, 8000);
                r.outcome = TweakOutcome::Applied;
                r.detail = "cleared Taskband\\Favorites";
                return r;
            });

    if (id == "disable-password-expiry")
        return std::make_unique<CmdTweak>(
            info("Disable password expiry", "net accounts /maxpwage:unlimited + "
                 "WMIC UserAccount PasswordExpires=false", true),
            [] {
                Proc p = run("net", {"accounts"});
                return p.out.contains("Maximum password age (days):") &&
                               (p.out.contains("Unlimited") || p.out.contains("UNLIMITED"))
                           ? TweakState::Applied
                           : TweakState::NotApplied;
            },
            [](const TweakArgs&) {
                TweakResult r{"disable-password-expiry", TweakOutcome::Failed, {}};
                Proc a = run("net", {"accounts", "/maxpwage:unlimited"});
                run("wmic", {"UserAccount", "set", "PasswordExpires=false"}, 30000);
                r.outcome = a.code == 0 ? TweakOutcome::Applied : TweakOutcome::Failed;
                r.detail = a.out.trimmed().toStdString();
                return r;
            });

    if (id == "disable-fast-startup")
        return std::make_unique<DwordTweak>(
            info("Disable fast startup", "HKLM Power: HiberbootEnabled = 0", true),
            [] {
                return hklm("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power");
            },
            "HiberbootEnabled", 0);

    if (id == "set-power-high-performance")
        return std::make_unique<CmdTweak>(
            info("Power plan: High performance", "powercfg /setactive SCHEME_MIN", true),
            [] {
                Proc p = run("powercfg", {"/getactivescheme"});
                return p.out.contains("High performance", Qt::CaseInsensitive)
                           ? TweakState::Applied
                           : TweakState::NotApplied;
            },
            [](const TweakArgs&) {
                TweakResult r{"set-power-high-performance", TweakOutcome::Failed, {}};
                Proc p = run("powercfg", {"/setactive", "SCHEME_MIN"});
                r.outcome = p.code == 0 ? TweakOutcome::Applied : TweakOutcome::Failed;
                r.detail = p.out.trimmed().toStdString();
                return r;
            });

    if (id == "disable-hibernate")
        return std::make_unique<CmdTweak>(
            info("Disable hibernate", "powercfg /hibernate off", true),
            [] { return TweakState::Unknown; },
            [](const TweakArgs&) {
                TweakResult r{"disable-hibernate", TweakOutcome::Failed, {}};
                Proc p = run("powercfg", {"/hibernate", "off"});
                r.outcome = p.code == 0 ? TweakOutcome::Applied : TweakOutcome::Failed;
                r.detail = p.out.trimmed().toStdString();
                return r;
            });

    if (id == "set-timezone")
        return std::make_unique<CmdTweak>(
            info("Set time zone", "tzutil /s <id>", true, {"id"}),
            [] { return TweakState::Unknown; },
            [](const TweakArgs& a) {
                TweakResult r{"set-timezone", TweakOutcome::Failed, {}};
                const QString tz = QString::fromStdString(a.get("id"));
                Proc cur = run("tzutil", {"/g"});
                if (cur.out.trimmed() == tz) {
                    r.outcome = TweakOutcome::AlreadyApplied;
                    return r;
                }
                Proc p = run("tzutil", {"/s", tz});
                r.outcome = p.code == 0 ? TweakOutcome::Applied : TweakOutcome::Failed;
                r.detail = p.out.trimmed().toStdString();
                return r;
            });

    if (id == "enable-rdp")
        return std::make_unique<CmdTweak>(
            info("Enable Remote Desktop", "HKLM Terminal Server fDenyTSConnections = 0 "
                 "+ firewall rule group 'Remote Desktop'", true),
            [] {
                QSettings s = hklm("SYSTEM\\CurrentControlSet\\Control\\Terminal Server");
                return s.value("fDenyTSConnections", 1).toInt() == 0 ? TweakState::Applied
                                                                     : TweakState::NotApplied;
            },
            [](const TweakArgs&) {
                TweakResult r{"enable-rdp", TweakOutcome::Failed, {}};
                QSettings s = hklm("SYSTEM\\CurrentControlSet\\Control\\Terminal Server");
                s.setValue("fDenyTSConnections", 0);
                s.sync();
                run("netsh", {"advfirewall", "firewall", "set", "rule",
                              "group=remote desktop", "new", "enable=Yes"});
                r.outcome = s.status() == QSettings::NoError ? TweakOutcome::Applied
                                                            : TweakOutcome::Failed;
                r.detail = "fDenyTSConnections=0 + firewall";
                return r;
            });

    if (id == "set-computer-name")
        return std::make_unique<CmdTweak>(
            info("Set computer name", "Rename-Computer (reboot required)", true, {"name"}),
            [] { return TweakState::Unknown; },
            [](const TweakArgs& a) {
                TweakResult r{"set-computer-name", TweakOutcome::Failed, {}};
                const QString nm = QString::fromStdString(a.get("name"));
                if (QString::fromLocal8Bit(qgetenv("COMPUTERNAME")).compare(
                        nm, Qt::CaseInsensitive) == 0) {
                    r.outcome = TweakOutcome::AlreadyApplied;
                    return r;
                }
                Proc p = run("powershell",
                             {"-NoProfile", "-NonInteractive", "-Command",
                              QString("Rename-Computer -NewName '%1' -Force").arg(nm)},
                             60000);
                r.outcome = p.code == 0 ? TweakOutcome::RequiresReboot : TweakOutcome::Failed;
                r.detail = p.out.trimmed().toStdString();
                return r;
            });

    if (id == "clean-startup-items" || id == "disable-startup-item")
        return std::make_unique<CmdTweak>(
            info(id == "clean-startup-items" ? "Clean startup items"
                                             : "Disable a startup item",
                 "Disable Run entries via StartupApproved bytes", true,
                 id == "disable-startup-item" ? std::vector<std::string>{"match"}
                                              : std::vector<std::string>{}),
            [] { return TweakState::Unknown; },
            [id](const TweakArgs& a) {
                TweakResult r{id, TweakOutcome::Failed, {}};
                const QString match =
                    id == "disable-startup-item"
                        ? QString::fromStdString(a.get("match"))
                        : QString();
                // Disable by writing the "disabled" bytes to StartupApproved.
                const QString ps = QString(R"(
$paths = @(
  'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run',
  'HKLM:\Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run'
)
$run = @(
  'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run',
  'HKLM:\Software\Microsoft\Windows\CurrentVersion\Run'
)
$disabled=[byte[]](0x03,0x00,0x00,0x00,0,0,0,0,0,0,0,0)
$n=0
for ($i=0;$i -lt $run.Count;$i++){
  if (-not (Test-Path $run[$i])) { continue }
  $props=(Get-Item $run[$i]).Property
  if (-not (Test-Path $paths[$i])) { New-Item -Path $paths[$i] -Force | Out-Null }
  foreach ($p in $props) {
    if ('%1' -ne '' -and $p -notlike '*%1*') { continue }
    Set-ItemProperty -Path $paths[$i] -Name $p -Value $disabled -Type Binary
    $n++
  }
}
"disabled $n"
)").arg(match);
                Proc p = run("powershell", {"-NoProfile", "-NonInteractive", "-Command", ps},
                             45000);
                r.outcome = p.code == 0 ? TweakOutcome::Applied : TweakOutcome::Failed;
                r.detail = p.out.trimmed().toStdString();
                return r;
            });

    return nullptr;
}

} // namespace

std::vector<TweakInfo> catalog() {
    static const char* ids[] = {
        "disable-password-expiry", "clean-taskbar-pins", "clean-startup-items",
        "disable-startup-item", "show-file-extensions", "show-hidden-files",
        "disable-fast-startup", "set-power-high-performance", "disable-hibernate",
        "set-timezone", "enable-rdp", "set-computer-name"};
    std::vector<TweakInfo> out;
    for (const char* id : ids) {
        if (auto t = makeTweak(id)) out.push_back(t->info());
    }
    return out;
}

bool isKnownTweak(const std::string& id) { return makeTweak(id) != nullptr; }

TweakResult runTweak(const std::string& id, const TweakArgs& args, bool elevated) {
    TweakResult r{id, TweakOutcome::Failed, {}};
    auto tweak = makeTweak(id);
    if (!tweak) {
        r.detail = "unknown tweak id";
        return r;
    }
    const TweakInfo i = tweak->info();

    for (const auto& req : i.requiredArgs) {
        if (!args.has(req)) {
            r.detail = "missing required arg '" + req + "'";
            return r;
        }
    }
    if (i.needsElevation && !elevated) {
        r.outcome = TweakOutcome::Skipped;
        r.detail = "needs Administrator";
        return r;
    }

    if (tweak->check() == TweakState::Applied) {
        r.outcome = TweakOutcome::AlreadyApplied;
        return r;
    }
    return tweak->apply(args);
}

} // namespace shiftech::core::config
