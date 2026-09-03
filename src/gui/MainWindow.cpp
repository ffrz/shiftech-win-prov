#include "MainWindow.h"

#include "../core/system/SystemInspector.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

using namespace shiftech::core;

namespace shiftech::gui {

namespace {
QColor severityColor(int sev) {
    switch (sev) {
        case 1: return QColor(0x2e, 0x7d, 0x32);  // success
        case 2: return QColor(0xef, 0x6c, 0x00);  // warning
        case 3: return QColor(0xc6, 0x28, 0x28);  // error
        default: return QColor(0x42, 0x42, 0x42); // info
    }
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Shiftech Win Provisioner");
    resize(760, 640);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    // --- System panel ---
    auto* sysGroup = new QGroupBox("System", central);
    auto* sysLayout = new QVBoxLayout(sysGroup);
    m_systemLabel = new QLabel(sysGroup);
    sysLayout->addWidget(m_systemLabel);
    root->addWidget(sysGroup);

    const system::SystemInfo info = system::SystemInspector::inspect();
    m_systemLabel->setText(
        QString("%1  (build %2)\nArchitecture: %3\nElevated: %4    winget: %5    pnputil: %6")
            .arg(QString::fromStdString(info.productName))
            .arg(info.buildNumber)
            .arg(info.arch == system::Arch::X64 ? "x64"
                 : info.arch == system::Arch::X86 ? "x86" : "unsupported")
            .arg(info.elevated ? "yes" : "no")
            .arg(info.wingetAvailable ? "yes" : "no")
            .arg(info.pnputilAvailable ? "yes" : "no"));

    // --- Options ---
    auto* optGroup = new QGroupBox("Run", central);
    auto* optForm = new QFormLayout(optGroup);
    m_profileBox = new QComboBox(optGroup);
    m_dryRunBox = new QCheckBox("Dry run (no changes)", optGroup);
    m_dryRunBox->setChecked(true);
    m_skipDriversBox = new QCheckBox("Skip drivers", optGroup);
    m_skipAppsBox = new QCheckBox("Skip applications", optGroup);
    optForm->addRow("Profile:", m_profileBox);
    optForm->addRow("", m_dryRunBox);
    optForm->addRow("", m_skipDriversBox);
    optForm->addRow("", m_skipAppsBox);
    root->addWidget(optGroup);
    populateProfiles();

    // --- Progress ---
    auto* progGroup = new QGroupBox("Progress", central);
    auto* progForm = new QFormLayout(progGroup);
    m_driverBar = new QProgressBar(progGroup);
    m_appBar = new QProgressBar(progGroup);
    m_currentTask = new QLabel("Idle", progGroup);
    progForm->addRow("Drivers:", m_driverBar);
    progForm->addRow("Applications:", m_appBar);
    progForm->addRow("Current task:", m_currentTask);
    root->addWidget(progGroup);

    // --- Buttons ---
    auto* btnRow = new QHBoxLayout();
    m_startBtn = new QPushButton("Start", central);
    m_cancelBtn = new QPushButton("Cancel", central);
    m_cancelBtn->setEnabled(false);
    m_saveReportBtn = new QPushButton("Save report…", central);
    m_saveReportBtn->setEnabled(false);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_saveReportBtn);
    root->addLayout(btnRow);

    // --- Log + report ---
    m_log = new QPlainTextEdit(central);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    root->addWidget(m_log, 2);

    m_report = new QPlainTextEdit(central);
    m_report->setReadOnly(true);
    m_report->setPlaceholderText("The provisioning report appears here when the run finishes.");
    root->addWidget(m_report, 2);

    setCentralWidget(central);

    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancel);
    connect(m_saveReportBtn, &QPushButton::clicked, this, &MainWindow::onSaveReport);
    connect(&m_engine, &EngineController::logEvent, this, &MainWindow::onLogEvent);
    connect(&m_engine, &EngineController::stageChanged, this, &MainWindow::onStageChanged);
    connect(&m_engine, &EngineController::progress, this, &MainWindow::onProgress);
    connect(&m_engine, &EngineController::finished, this, &MainWindow::onFinished);
}

void MainWindow::populateProfiles() {
    m_profileBox->clear();
    const QString exeDir = QCoreApplication::applicationDirPath();
    for (const QString& d : {exeDir + "/profiles", exeDir + "/../profiles",
                             exeDir + "/../../profiles"}) {
        QDir dir(d);
        if (!dir.exists()) continue;
        for (const QFileInfo& fi : dir.entryInfoList({"*.json"}, QDir::Files, QDir::Name)) {
            m_profileBox->addItem(fi.completeBaseName(), fi.absoluteFilePath());
        }
        if (m_profileBox->count() > 0) break;
    }
    if (m_profileBox->count() == 0) m_profileBox->addItem("(no profiles found)", QString());
}

void MainWindow::setRunning(bool running) {
    m_startBtn->setEnabled(!running);
    m_cancelBtn->setEnabled(running);
    m_profileBox->setEnabled(!running);
    m_dryRunBox->setEnabled(!running);
    m_skipDriversBox->setEnabled(!running);
    m_skipAppsBox->setEnabled(!running);
}

void MainWindow::onStart() {
    m_log->clear();
    m_report->clear();
    m_driverBar->setValue(0);
    m_appBar->setValue(0);
    m_saveReportBtn->setEnabled(false);

    core::provisioning::ProvisioningOptions o;
    const QString profilePath = m_profileBox->currentData().toString();
    if (!profilePath.isEmpty()) o.profile = profilePath.toStdString();
    o.dryRun = m_dryRunBox->isChecked();
    o.skipDrivers = m_skipDriversBox->isChecked();
    o.skipApps = m_skipAppsBox->isChecked() || profilePath.isEmpty();

    setRunning(true);
    m_currentTask->setText("Starting…");
    m_engine.start(o);
}

void MainWindow::onCancel() {
    m_currentTask->setText("Cancelling…");
    m_engine.requestCancel();
}

void MainWindow::onLogEvent(QString isoTime, int severity, QString category, QString message,
                            int progress) {
    Q_UNUSED(progress);
    const QString ts = isoTime.mid(11, 8);
    auto* c = m_log;
    QTextCharFormat fmt;
    fmt.setForeground(severityColor(severity));
    QTextCursor cur = c->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(QString("[%1] %2  %3\n").arg(ts, category, message), fmt);
    c->setTextCursor(cur);
    m_currentTask->setText(message);
}

void MainWindow::onStageChanged(QString stage) {
    m_currentTask->setText("Stage: " + stage);
}

void MainWindow::onProgress(QString category, int percent) {
    if (category == "driver") m_driverBar->setValue(percent);
    else if (category == "application") m_appBar->setValue(percent);
}

void MainWindow::onFinished(QString status, QString reportText, QString runDir) {
    setRunning(false);
    m_currentTask->setText("Done — " + status);
    m_report->setPlainText(reportText);
    m_lastReportRunDir = runDir;
    m_saveReportBtn->setEnabled(true);
    m_driverBar->setValue(100);
    m_appBar->setValue(100);
}

void MainWindow::onSaveReport() {
    if (m_lastReportRunDir.isEmpty()) return;
    const QString src = QDir(m_lastReportRunDir).filePath("run.json");
    const QString dst = QFileDialog::getSaveFileName(this, "Save report", "report.json",
                                                     "JSON (*.json)");
    if (dst.isEmpty()) return;
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly)) return;
    const QJsonObject run = QJsonDocument::fromJson(in.readAll()).object();
    QFile out(dst);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        out.write(QJsonDocument(run.value("report").toObject()).toJson(QJsonDocument::Indented));
    }
}

} // namespace shiftech::gui
