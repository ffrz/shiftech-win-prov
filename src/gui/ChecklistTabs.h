#pragma once

#include "../core/profiles/Profile.h"
#include <QWidget>

class QCheckBox;
class QLabel;
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

    core::profiles::Profile m_seed;   // keeps name/description + the provider order (not
                                      // editable in the GUI - set it in the profile JSON)

    // Drivers
    QCheckBox* m_driversEnabled = nullptr;
    QLabel* m_providerOrderLabel = nullptr;  // read-only display of the profile's chain
    QCheckBox* m_installUnsigned = nullptr;
    QTableWidget* m_deviceTable = nullptr;   // [install?] | device | hardware id  (read-only)

    // Applications
    QTableWidget* m_appTable = nullptr;      // [install?] | application | source  (read-only)

    // Config
    QTableWidget* m_configTable = nullptr;   // [apply?] | tweak  (read-only; args/admin in tooltip)
};

} // namespace shiftech::gui
