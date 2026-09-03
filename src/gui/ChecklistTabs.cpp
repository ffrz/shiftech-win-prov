#include "ChecklistTabs.h"

#include "../core/applications/LocalInstallerProvider.h"
#include "../core/config/ConfigTweak.h"
#include "../core/hardware/DeviceEnumerator.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <set>

using namespace shiftech::core;

namespace shiftech::gui {

namespace {

QCheckBox* cellCheck(QTableWidget* t, int row, bool checked) {
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
    return t->cellWidget(row, 0)->findChild<QCheckBox*>();
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

        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Provider order:"));
        m_providerOrder = new QLineEdit("localcache,windowsupdate,mirror", drivers);
        row->addWidget(m_providerOrder);
        l->addLayout(row);

        m_installUnsigned = new QCheckBox("Install unsigned driver packages (not recommended)",
                                          drivers);
        l->addWidget(m_installUnsigned);

        l->addWidget(new QLabel("Devices needing a driver (untick to skip):"));
        m_deviceTable = new QTableWidget(0, 3, drivers);
        m_deviceTable->setHorizontalHeaderLabels({"", "Device", "Hardware ID"});
        m_deviceTable->horizontalHeader()->setStretchLastSection(true);
        m_deviceTable->verticalHeader()->setVisible(false);
        l->addWidget(m_deviceTable);
    }

    // --- Applications tab ---
    {
        auto* l = new QVBoxLayout(apps);
        l->addWidget(new QLabel("Tick the applications to install:"));
        m_appTable = new QTableWidget(0, 4, apps);
        m_appTable->setHorizontalHeaderLabels({"", "Application", "Source", "Required"});
        m_appTable->horizontalHeader()->setStretchLastSection(true);
        m_appTable->verticalHeader()->setVisible(false);
        l->addWidget(m_appTable);
    }

    // --- Config tab ---
    {
        auto* l = new QVBoxLayout(config);
        l->addWidget(new QLabel("Tick the Windows tweaks to apply:"));
        m_configTable = new QTableWidget(0, 4, config);
        m_configTable->setHorizontalHeaderLabels({"", "Tweak", "", "Args"});
        m_configTable->horizontalHeader()->setStretchLastSection(true);
        m_configTable->verticalHeader()->setVisible(false);
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
    m_providerOrder->setText(m_seed.drivers.providerOrder.empty()
                                 ? "localcache,windowsupdate,mirror"
                                 : QString::fromStdString(m_seed.drivers.providerOrder));
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
        cellCheck(m_deviceTable, r, !isExcluded);
        m_deviceTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(d.name)));
        m_deviceTable->setItem(
            r, 2,
            new QTableWidgetItem(d.hardwareIds.empty()
                                     ? QString()
                                     : QString::fromStdString(d.hardwareIds.front())));
        // stash the instance id on the row for read-back
        m_deviceTable->item(r, 1)->setData(Qt::UserRole,
                                           QString::fromStdString(d.instanceId));
        m_deviceTable->item(r, 2)->setData(Qt::UserRole + 1,
                                           d.hardwareIds.empty()
                                               ? QString()
                                               : QString::fromStdString(d.hardwareIds.front()));
    }
}

void ChecklistTabs::buildAppsTab() {
    // Union: profile apps + local apps available on the medium.
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
        cellCheck(m_appTable, r, a.enabled);
        auto* idItem = new QTableWidgetItem(QString::fromStdString(a.id));
        idItem->setData(Qt::UserRole, QString::fromStdString(a.wingetId));
        idItem->setData(Qt::UserRole + 1,
                        a.source == profiles::AppSource::Local ? "local" : "winget");
        m_appTable->setItem(r, 1, idItem);
        m_appTable->setItem(
            r, 2,
            new QTableWidgetItem(a.source == profiles::AppSource::Local ? "local drive"
                                                                       : "winget"));
        auto* reqCb = new QCheckBox;
        reqCb->setChecked(a.required);
        m_appTable->setCellWidget(r, 3, reqCb);
    }
}

void ChecklistTabs::buildConfigTab() {
    std::map<std::string, profiles::ConfigEntry> fromProfile;
    for (const auto& c : m_seed.config) fromProfile[c.id] = c;

    m_configTable->setRowCount(0);
    for (const auto& t : config::catalog()) {
        const int r = m_configTable->rowCount();
        m_configTable->insertRow(r);
        const bool checked =
            fromProfile.count(t.id) && fromProfile.at(t.id).enabled;
        cellCheck(m_configTable, r, checked);

        auto* nameItem = new QTableWidgetItem(QString::fromStdString(t.title));
        nameItem->setData(Qt::UserRole, QString::fromStdString(t.id));
        nameItem->setToolTip(QString::fromStdString(t.description));
        m_configTable->setItem(r, 1, nameItem);
        m_configTable->setItem(r, 2,
                               new QTableWidgetItem(t.needsElevation ? "admin" : ""));

        if (!t.requiredArgs.empty()) {
            auto* edit = new QLineEdit;
            edit->setPlaceholderText(
                QString::fromStdString("args: " + [&] {
                    std::string s;
                    for (const auto& k : t.requiredArgs) s += (s.empty() ? "" : ", ") + k;
                    return s;
                }()));
            if (fromProfile.count(t.id) && !fromProfile.at(t.id).args.empty()) {
                // show the first arg value (most tweaks take one)
                edit->setText(QString::fromStdString(
                    fromProfile.at(t.id).args.begin()->second));
            }
            m_configTable->setCellWidget(r, 3, edit);
            // keep the arg key so read-back can pair it
            m_configTable->item(r, 1)->setData(Qt::UserRole + 1,
                                               QString::fromStdString(t.requiredArgs.front()));
        }
    }
}

core::profiles::Profile ChecklistTabs::effectiveProfile() const {
    profiles::Profile p;
    p.name = m_seed.name.empty() ? "custom" : m_seed.name;
    p.description = m_seed.description;

    p.drivers.enabled = m_driversEnabled->isChecked();
    p.drivers.providerOrder = m_providerOrder->text().trimmed().toStdString();
    p.drivers.installUnsigned = m_installUnsigned->isChecked();
    for (int r = 0; r < m_deviceTable->rowCount(); ++r) {
        if (!checkAt(m_deviceTable, r)->isChecked()) {
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
        if (auto* rc = qobject_cast<QCheckBox*>(m_appTable->cellWidget(r, 3)))
            e.required = rc->isChecked();
        p.applications.push_back(e);
    }

    for (int r = 0; r < m_configTable->rowCount(); ++r) {
        profiles::ConfigEntry e;
        auto* nameItem = m_configTable->item(r, 1);
        e.id = nameItem->data(Qt::UserRole).toString().toStdString();
        e.enabled = checkAt(m_configTable, r)->isChecked();
        const QString argKey = nameItem->data(Qt::UserRole + 1).toString();
        if (!argKey.isEmpty()) {
            if (auto* le = qobject_cast<QLineEdit*>(m_configTable->cellWidget(r, 3))) {
                const QString v = le->text().trimmed();
                if (!v.isEmpty()) e.args[argKey.toStdString()] = v.toStdString();
            }
        }
        p.config.push_back(e);
    }

    return p;
}

void ChecklistTabs::setEnabledForRun(bool editable) {
    setEnabled(editable);
}

} // namespace shiftech::gui
