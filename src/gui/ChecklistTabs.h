#pragma once

#include "../core/profiles/Profile.h"
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QTableWidget;

namespace shiftech::gui {

// The Drivers / Applications / Config picker. Seed it from a Profile; read back an
// effective Profile reflecting the current ticked state. No pipeline logic here.
class ChecklistTabs : public QWidget {
    Q_OBJECT
public:
    explicit ChecklistTabs(QWidget* parent = nullptr);

    // Populate all three tabs from a profile (its name/description are kept).
    void seed(const core::profiles::Profile& profile);

    // Build a Profile from the current UI state.
    core::profiles::Profile effectiveProfile() const;

    void setEnabledForRun(bool editable);

signals:
    void selectionChanged();

private:
    void buildDriversTab();
    void buildAppsTab();
    void buildConfigTab();

    core::profiles::Profile m_seed;   // keeps name/description + rows we didn't surface

    // Drivers
    QCheckBox* m_driversEnabled = nullptr;
    QLineEdit* m_providerOrder = nullptr;
    QCheckBox* m_installUnsigned = nullptr;
    QTableWidget* m_deviceTable = nullptr;   // device -> "resolve" checkbox (unchecked = exclude)

    // Applications
    QTableWidget* m_appTable = nullptr;      // checkbox | id | source | required

    // Config
    QTableWidget* m_configTable = nullptr;   // checkbox | id | title | [admin] | args field
};

} // namespace shiftech::gui
