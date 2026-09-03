#pragma once

#include "EngineController.h"
#include <QMainWindow>

class QComboBox;
class QCheckBox;
class QLabel;
class QProgressBar;
class QPlainTextEdit;
class QPushButton;

namespace shiftech::gui {

class ChecklistTabs;

// Pure front-end over EngineController. The three checklist tabs build an effective
// Profile; the window hands it to the engine and renders the event signals.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onProfilePicked(int index);
    void onStart();
    void onCancel();
    void onLogEvent(QString isoTime, int severity, QString category, QString message, int progress);
    void onStageChanged(QString stage);
    void onProgress(QString stage, int percent);
    void onFinished(QString status, QString reportText, QString runDir);
    void onSaveReport();

private:
    void populateProfiles();
    void setRunning(bool running);

    EngineController m_engine;

    QLabel* m_systemLabel = nullptr;
    QComboBox* m_profileBox = nullptr;
    ChecklistTabs* m_tabs = nullptr;
    QCheckBox* m_dryRunBox = nullptr;
    QProgressBar* m_driverBar = nullptr;
    QProgressBar* m_appBar = nullptr;
    QLabel* m_currentTask = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QPushButton* m_saveReportBtn = nullptr;
    QPlainTextEdit* m_log = nullptr;
    QPlainTextEdit* m_report = nullptr;

    QString m_lastReportRunDir;
};

} // namespace shiftech::gui
