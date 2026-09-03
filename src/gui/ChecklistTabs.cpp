#include "ChecklistTabs.h"

#include "../core/applications/LocalInstallerProvider.h"
#include "../core/config/ConfigTweak.h"
#include "../core/hardware/DeviceEnumerator.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <set>

using namespace shiftech::core;

namespace shiftech::gui {

namespace {

// A centred checkbox in column 0. The cell itself is never editable.
QCheckBox* checkCell(QTableWidget* t, int row, bool checked) {
    auto* cb = new QCheckBox;
    cb->setChecked(checked);
    auto* w = new QWidget;
    auto* l = new QHBoxLayout(w);
    l->addWidget(cb);
    l->setAlignment(Qt::AlignCenter);
    l->setContentsMargins(0, 0, 0, 0);
    t->setCellWidget(row, 0, w);
    return cb;
}
QCheckBox* checkAt(QTableWidget* t, int row) {
    QWidget* w = t->cellWidget(row, 0);
    return w ? w->findChild<QCheckBox*>() : nullptr;
}

// A read-only text cell (selectable, not editable).
QTableWidgetItem* textCell(const QString& text) {
    auto* it = new QTableWidgetItem(text);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return it;
}

// Lock a table down: no edit triggers, no in-place editing of any item.
void makeReadOnly(QTableWidget* t) {
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->verticalHeader()->setVisible(false);
    t->setAlternatingRowColors(true);
}

} // namespace

ChecklistTabs::ChecklistTabs(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    root->addWidget(tabs);

    auto* drivers = new QWidget;
    auto* apps = new QWidget;
    auto* config = new QWidget;
    tabs->addTab(drivers, "Drivers");
    tabs->addTab(apps, "Applications");
    tabs->addTab(config, "Config");

    // --- Drivers tab ---
    {
        auto* l = new QVBoxLayout(drivers);
        m_driversEnabled = new QCheckBox("Run the driver stage", drivers);
        m_driversEnabled->setChecked(true);
        l->addWidget(m_driversEnabled);

        m_providerOrderLabel = new QLabel(drivers);
        m_providerOrderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        l->addWidget(m_providerOrderLabel);

        m_installUnsigned =
            new QCheckBox("Install unsigned driver packages (not recommended)", drivers);
        l->addWidget(m_installUnsigned);

        auto* hint = new QLabel(
            "Devices Windows can't drive. Tick = try to resolve a driver, "
            "untick = leave it alone.", drivers);
        hint->setWordWrap(true);
        hint->setStyleSheet("color: #666;");
        l->addWidget(hint);

        m_deviceTable = new QTableWidget(0, 3, drivers);
        m_deviceTable->setHorizontalHeaderLabels({"Install?", "Device", "Hardware ID"});
        m_deviceTable->horizontalHeader()->setStretchLastSection(true);
        m_deviceTable->setColumnWidth(0, 60);
        makeReadOnly(m_deviceTable);
        l->addWidget(m_deviceTable);
    }

    // --- Applications tab ---
    {
        auto* l = new QVBoxLayout(apps);
        auto* hint = new QLabel(
            "Tick the apps to install. Source \"winget\" = downloaded via Windows Package "
            "Manager (needs internet); \"local drive\" = installer bundled in apps\\<id>\\ "
            "on this medium. To mark an app as required (a failure flags the run), set "
            "\"required\": true in the profile file.", apps);
        hint->setWordWrap(true);
        hint->setStyleSheet("color: #666;");
        l->addWidget(hint);

        m_appTable = new QTableWidget(0, 3, apps);
        m_appTable->setHorizontalHeaderLabels({"Install?", "Application", "Source"});
        m_appTable->horizontalHeader()->setStretchLastSection(false);
        m_appTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_appTable->setColumnWidth(0, 60);
        m_appTable->setColumnWidth(2, 110);
        makeReadOnly(m_appTable);
        l->addWidget(m_appTable);
    }

    // --- Config tab ---
    {
        auto* l = new QVBoxLayout(config);
        auto* hint = new QLabel(
            "Tick the Windows tweaks to apply. Hover a row for what it changes. Tweaks that "
            "change machine-wide settings need the app to run as Administrator - they are "
            "skipped (with a note in the log) otherwise. A few tweaks need a value "
            "(time-zone id, computer name); set it in the profile file's \"config\" section "
            "- those are skipped if it's missing.", config);
        hint->setWordWrap(true);
        hint->setStyleSheet("color: #666;");
        l->addWidget(hint);

        m_configTable = new QTableWidget(0, 2, config);
        m_configTable->setHorizontalHeaderLabels({"Apply?", "Tweak"});
        m_configTable->horizontalHeader()->setStretchLastSection(true);
        m_configTable->setColumnWidth(0, 60);
        makeReadOnly(m_configTable);
        l->addWidget(m_configTable);
    }
}

void ChecklistTabs::seed(const profiles::Profile& profile) {
    m_seed = profile;
    buildDriversTab();
    buildAppsTab();
    buildConfigTab();
}

void ChecklistTabs::buildDriversTab() {
    m_driversEnabled->setChecked(m_seed.drivers.enabled);
    const QString order = m_seed.drivers.providerOrder.empty()
                              ? "localcache, windowsupdate, mirror  (engine default)"
                              : QString::fromStdString(m_seed.drivers.providerOrder);
    m_providerOrderLabel->setText("Provider order (set in the profile file): " + order);
    m_installUnsigned->setChecked(m_seed.drivers.installUnsigned);

    hardware::DeviceEnumerator en;
    const auto needing = en.enumerateNeedingDriver();
    const std::set<std::string> excluded(m_seed.drivers.exclude.begin(),
                                         m_seed.drivers.exclude.end());

    m_deviceTable->setRowCount(0);
    for (const auto& d : needing) {
        const int r = m_deviceTable->rowCount();
        m_deviceTable->insertRow(r);

        bool isExcluded = excluded.count(d.instanceId) > 0;
        for (const auto& h : d.hardwareIds) if (excluded.count(h)) isExcluded = true;
        checkCell(m_deviceTable, r, !isExcluded);

        auto* nameItem = textCell(QString::fromStdString(d.name));
        nameItem->setData(Qt::UserRole, QString::fromStdString(d.instanceId));
        m_deviceTable->setItem(r, 1, nameItem);
        m_deviceTable->setItem(
            r, 2,
            textCell(d.hardwareIds.empty() ? QString()
                                           : QString::fromStdString(d.hardwareIds.front())));
    }
}

void ChecklistTabs::buildAppsTab() {
    applications::LocalInstallerProvider local;
    std::vector<profiles::AppEntry> rows = m_seed.applications;
    std::set<std::string> have;
    for (const auto& a : rows) have.insert(a.id);
    for (const auto& m : local.available()) {
        if (have.count(m.id)) continue;
        profiles::AppEntry e;
        e.id = m.id;
        e.source = profiles::AppSource::Local;
        e.enabled = false;
        rows.push_back(e);
    }

    m_appTable->setRowCount(0);
    for (const auto& a : rows) {
        const int r = m_appTable->rowCount();
        m_appTable->insertRow(r);
        checkCell(m_appTable, r, a.enabled);

        auto* idItem = textCell(QString::fromStdString(a.id));
        idItem->setData(Qt::UserRole, QString::fromStdString(a.wingetId));
        idItem->setData(Qt::UserRole + 1,
                        a.source == profiles::AppSource::Local ? "local" : "winget");
        idItem->setData(Qt::UserRole + 2, a.required);  // carried through, not shown/edited
        if (a.required) idItem->setToolTip("required by the profile");
        m_appTable->setItem(r, 1, idItem);
        m_appTable->setItem(
            r, 2,
            textCell(a.source == profiles::AppSource::Local ? "local drive" : "winget"));
    }
}

void ChecklistTabs::buildConfigTab() {
    // Only the tweaks the profile actually lists are shown. Args + admin are the profile
    // author's concern; the technician just ticks which to apply.
    m_configTable->setRowCount(0);
    for (const auto& c : m_seed.config) {
        // resolve the title/description from the catalog (fall back to the id)
        QString title = QString::fromStdString(c.id);
        QString desc;
        for (const auto& t : config::catalog()) {
            if (t.id == c.id) {
                title = QString::fromStdString(t.title);
                desc = QString::fromStdString(t.description);
                if (t.needsElevation) desc += "\n(needs Administrator)";
                break;
            }
        }
        const int r = m_configTable->rowCount();
        m_configTable->insertRow(r);
        checkCell(m_configTable, r, c.enabled);
        auto* nameItem = textCell(title);
        nameItem->setData(Qt::UserRole, QString::fromStdString(c.id));
        if (!desc.isEmpty()) nameItem->setToolTip(desc);
        m_configTable->setItem(r, 1, nameItem);
    }
    if (m_seed.config.empty()) {
        m_configTable->insertRow(0);
        m_configTable->setSpan(0, 0, 1, 2);
        m_configTable->setItem(
            0, 0,
            textCell("This profile has no config tweaks. Add them in the profile file."));
    }
}

core::profiles::Profile ChecklistTabs::effectiveProfile() const {
    profiles::Profile p;
    p.name = m_seed.name.empty() ? "custom" : m_seed.name;
    p.description = m_seed.description;

    // Provider order is NOT editable in the GUI - carry the profile's value through.
    p.drivers.enabled = m_driversEnabled->isChecked();
    p.drivers.providerOrder = m_seed.drivers.providerOrder;
    p.drivers.installUnsigned = m_installUnsigned->isChecked();
    for (int r = 0; r < m_deviceTable->rowCount(); ++r) {
        auto* cb = checkAt(m_deviceTable, r);
        if (cb && !cb->isChecked()) {
            const QString inst = m_deviceTable->item(r, 1)->data(Qt::UserRole).toString();
            if (!inst.isEmpty()) p.drivers.exclude.push_back(inst.toStdString());
        }
    }

    for (int r = 0; r < m_appTable->rowCount(); ++r) {
        profiles::AppEntry e;
        auto* idItem = m_appTable->item(r, 1);
        e.id = idItem->text().toStdString();
        e.wingetId = idItem->data(Qt::UserRole).toString().toStdString();
        e.source = idItem->data(Qt::UserRole + 1).toString() == "local"
                       ? profiles::AppSource::Local
                       : profiles::AppSource::WinGet;
        if (e.source == profiles::AppSource::WinGet && e.wingetId.empty()) e.wingetId = e.id;
        e.enabled = checkAt(m_appTable, r)->isChecked();
        e.required = idItem->data(Qt::UserRole + 2).toBool();  // from the profile, not the GUI
        p.applications.push_back(e);
    }

    // Config: keep the profile's entries, only toggle `enabled` from the ticks.
    // Args are never edited in the GUI - carry them straight through.
    std::map<std::string, bool> ticked;
    for (int r = 0; r < m_configTable->rowCount(); ++r) {
        auto* nameItem = m_configTable->item(r, 1);
        if (!nameItem) continue;
        const std::string id = nameItem->data(Qt::UserRole).toString().toStdString();
        if (id.empty()) continue;
        if (auto* cb = checkAt(m_configTable, r)) ticked[id] = cb->isChecked();
    }
    for (const auto& c : m_seed.config) {
        profiles::ConfigEntry e = c;
        auto it = ticked.find(c.id);
        if (it != ticked.end()) e.enabled = it->second;
        p.config.push_back(e);
    }

    return p;
}

void ChecklistTabs::setEnabledForRun(bool editable) { setEnabled(editable); }

} // namespace shiftech::gui
