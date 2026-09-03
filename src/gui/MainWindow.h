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

// Pure front-end over EngineController. No pipeline logic here — the window builds a
// ProvisioningOptions from the controls, starts the engine, and renders its signals.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
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
    QCheckBox* m_dryRunBox = nullptr;
    QCheckBox* m_skipDriversBox = nullptr;
    QCheckBox* m_skipAppsBox = nullptr;
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
