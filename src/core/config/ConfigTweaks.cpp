#include "ConfigTweak.h"

#include <QProcess>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <climits>
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

const char* toString(RevertOutcome o) {
    switch (o) {
        case RevertOutcome::Reverted: return "Reverted";
        case RevertOutcome::NothingToRevert: return "NothingToRevert";
        case RevertOutcome::Failed: return "Failed";
        case RevertOutcome::Skipped: return "Skipped";
        case RevertOutcome::NotSupported: return "NotSupported";
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

void restartExplorer() {
    run("taskkill", {"/f", "/im", "explorer.exe"}, 8000);
    run("cmd", {"/c", "start", "", "explorer.exe"}, 8000);
}

// --- generic HKCU/HKLM DWORD tweak --------------------------------------------
// wanted = value we set. defaultVal = the Windows default to restore on revert
// (INT_MIN => revert by deleting the value).
class DwordTweak : public ConfigTweak {
public:
    DwordTweak(TweakInfo i, std::function<QSettings()> reg, QString value, int wanted,
              int defaultVal, bool restartExplorerOnChange = false)
        : m_info(std::move(i)), m_reg(std::move(reg)), m_value(std::move(value)),
          m_wanted(wanted), m_default(defaultVal), m_restart(restartExplorerOnChange) {}

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
        if (m_restart) restartExplorer();
        r.outcome = TweakOutcome::Applied;
        r.detail = m_value.toStdString() + " = " + std::to_string(m_wanted);
        return r;
    }

    RevertResult revert(const TweakArgs&) override {
        RevertResult r{m_info.id, RevertOutcome::Failed, {}};
        QSettings s = m_reg();
        if (!s.contains(m_value) || s.value(m_value).toInt() != m_wanted) {
            r.outcome = RevertOutcome::NothingToRevert;
            return r;
        }
        if (m_default == INT_MIN) s.remove(m_value);
        else s.setValue(m_value, m_default);
        s.sync();
        if (s.status() != QSettings::NoError) {
            r.detail = "registry write failed";
            return r;
        }
        if (m_restart) restartExplorer();
        r.outcome = RevertOutcome::Reverted;
        r.detail = m_default == INT_MIN
                       ? (m_value.toStdString() + " removed")
                       : (m_value.toStdString() + " = " + std::to_string(m_default));
        return r;
    }

private:
    TweakInfo m_info;
    std::function<QSettings()> m_reg;
    QString m_value;
    int m_wanted;
    int m_default;
    bool m_restart;
};

// --- command-based tweak (net/powercfg/tzutil/...) -----------------------------
class CmdTweak : public ConfigTweak {
public:
    using CheckFn = std::function<TweakState()>;
    using ApplyFn = std::function<TweakResult(const TweakArgs&)>;
    using RevertFn = std::function<RevertResult(const TweakArgs&)>;

    CmdTweak(TweakInfo i, CheckFn checkFn, ApplyFn applyFn, RevertFn revertFn = {})
        : m_info(std::move(i)), m_check(std::move(checkFn)), m_apply(std::move(applyFn)),
          m_revert(std::move(revertFn)) {}
    TweakInfo info() const override { return m_info; }
    TweakState check() const override { return m_check ? m_check() : TweakState::Unknown; }
    TweakResult apply(const TweakArgs& a) override { return m_apply(a); }
    RevertResult revert(const TweakArgs& a) override {
        if (m_revert) return m_revert(a);
        return {m_info.id, RevertOutcome::NotSupported, "no automatic undo for this tweak"};
    }

private:
    TweakInfo m_info;
    CheckFn m_check;
    ApplyFn m_apply;
    RevertFn m_revert;
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

    // INT_MIN as the default => revert by deleting the value.


    if (id == "show-file-extensions")
        return std::make_unique<DwordTweak>(
            info("Show file extensions", "Explorer Advanced: HideFileExt = 0", false),
            [] { return hkcu(kExplorerAdv); }, "HideFileExt", 0, /*default*/ 1, /*restart*/ true);

    if (id == "show-hidden-files")
        return std::make_unique<DwordTweak>(
            info("Show hidden files", "Explorer Advanced: Hidden = 1", false),
            [] { return hkcu(kExplorerAdv); }, "Hidden", 1, /*default*/ 2, /*restart*/ true);

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
                restartExplorer();
                r.outcome = TweakOutcome::Applied;
                r.detail = "cleared Taskband\\Favorites";
                return r;
            },
            [](const TweakArgs&) -> RevertResult {
                // Removed pins can't be reconstructed - Windows will re-create defaults
                // on next sign-in.
                return {"clean-taskbar-pins", RevertOutcome::NotSupported,
                        "removed pins can't be restored; Windows re-adds defaults on sign-in"};
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
            },
            [](const TweakArgs&) {
                RevertResult r{"disable-password-expiry", RevertOutcome::Failed, {}};
                // Windows default max password age is 42 days.
                Proc a = run("net", {"accounts", "/maxpwage:42"});
                run("wmic", {"UserAccount", "set", "PasswordExpires=true"}, 30000);
                r.outcome = a.code == 0 ? RevertOutcome::Reverted : RevertOutcome::Failed;
                r.detail = "maxpwage=42, PasswordExpires=true";
                return r;
            });

    if (id == "disable-fast-startup")
        return std::make_unique<DwordTweak>(
            info("Disable fast startup", "HKLM Power: HiberbootEnabled = 0", true),
            [] {
                return hklm("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power");
            },
            "HiberbootEnabled", 0, /*default*/ 1);

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
            },
            [](const TweakArgs&) {
                RevertResult r{"set-power-high-performance", RevertOutcome::Failed, {}};
                // SCHEME_BALANCED is the Windows default.
                Proc p = run("powercfg", {"/setactive", "SCHEME_BALANCED"});
                r.outcome = p.code == 0 ? RevertOutcome::Reverted : RevertOutcome::Failed;
                r.detail = "active scheme -> Balanced";
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
            },
            [](const TweakArgs&) {
                RevertResult r{"disable-hibernate", RevertOutcome::Failed, {}};
                Proc p = run("powercfg", {"/hibernate", "on"});
                r.outcome = p.code == 0 ? RevertOutcome::Reverted : RevertOutcome::Failed;
                r.detail = "hibernate on";
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
    // set-timezone has no revert - there is no "default" time zone to go back to.

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
            },
            [](const TweakArgs&) {
                RevertResult r{"enable-rdp", RevertOutcome::Failed, {}};
                QSettings s = hklm("SYSTEM\\CurrentControlSet\\Control\\Terminal Server");
                s.setValue("fDenyTSConnections", 1);
                s.sync();
                run("netsh", {"advfirewall", "firewall", "set", "rule",
                              "group=remote desktop", "new", "enable=No"});
                r.outcome = s.status() == QSettings::NoError ? RevertOutcome::Reverted
                                                            : RevertOutcome::Failed;
                r.detail = "fDenyTSConnections=1 + firewall rule disabled";
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
    // set-computer-name has no revert - the previous name isn't recorded.

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
            },
            [id](const TweakArgs& a) {
                RevertResult r{id, RevertOutcome::Failed, {}};
                const QString match =
                    id == "disable-startup-item"
                        ? QString::fromStdString(a.get("match"))
                        : QString();
                // Re-enable by writing the "enabled" bytes.
                const QString ps = QString(R"(
$paths = @(
  'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run',
  'HKLM:\Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run'
)
$enabled=[byte[]](0x02,0x00,0x00,0x00,0,0,0,0,0,0,0,0)
$n=0
foreach ($pth in $paths) {
  if (-not (Test-Path $pth)) { continue }
  foreach ($p in (Get-Item $pth).Property) {
    if ('%1' -ne '' -and $p -notlike '*%1*') { continue }
    Set-ItemProperty -Path $pth -Name $p -Value $enabled -Type Binary
    $n++
  }
}
"enabled $n"
)").arg(match);
                Proc p = run("powershell",
                             {"-NoProfile", "-NonInteractive", "-Command", ps}, 45000);
                r.outcome = p.code == 0 ? RevertOutcome::Reverted : RevertOutcome::Failed;
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

RevertResult revertTweak(const std::string& id, const TweakArgs& args, bool elevated) {
    RevertResult r{id, RevertOutcome::Failed, {}};
    auto tweak = makeTweak(id);
    if (!tweak) {
        r.detail = "unknown tweak id";
        return r;
    }
    const TweakInfo i = tweak->info();
    if (i.needsElevation && !elevated) {
        r.outcome = RevertOutcome::Skipped;
        r.detail = "needs Administrator";
        return r;
    }
    return tweak->revert(args);
}

} // namespace shiftech::core::config
