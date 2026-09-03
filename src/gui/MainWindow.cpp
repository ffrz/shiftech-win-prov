#include "MainWindow.h"

#include "ChecklistTabs.h"
#include "../core/profiles/ProfileLoader.h"
#include "../core/system/SystemInspector.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
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
        case 1: return QColor(0x2e, 0x7d, 0x32);
        case 2: return QColor(0xef, 0x6c, 0x00);
        case 3: return QColor(0xc6, 0x28, 0x28);
        default: return QColor(0x42, 0x42, 0x42);
    }
}

profiles::Profile blankProfile() {
    profiles::Profile p;
    p.name = "custom";
    p.description = "Ad-hoc selection";
    p.drivers.enabled = true;
    return p;
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Shiftech Win Provisioner");
    resize(860, 760);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    // System panel
    auto* sysGroup = new QGroupBox("System", central);
    auto* sysLayout = new QVBoxLayout(sysGroup);
    m_systemLabel = new QLabel(sysGroup);
    sysLayout->addWidget(m_systemLabel);
    root->addWidget(sysGroup);

    const system::SystemInfo info = system::SystemInspector::inspect();
    m_systemLabel->setText(
        QString("%1  (build %2)   %3   Elevated: %4    winget: %5    pnputil: %6")
            .arg(QString::fromStdString(info.productName))
            .arg(info.buildNumber)
            .arg(info.arch == system::Arch::X64 ? "x64"
                 : info.arch == system::Arch::X86 ? "x86" : "unsupported")
            .arg(info.elevated ? "yes" : "no")
            .arg(info.wingetAvailable ? "yes" : "no")
            .arg(info.pnputilAvailable ? "yes" : "no"));

    // Profile row
    auto* pRow = new QHBoxLayout;
    pRow->addWidget(new QLabel("Profile:"));
    m_profileBox = new QComboBox(central);
    pRow->addWidget(m_profileBox, 1);
    m_dryRunBox = new QCheckBox("Dry run (no changes)", central);
    m_dryRunBox->setChecked(true);
    pRow->addWidget(m_dryRunBox);
    root->addLayout(pRow);

    auto* profHint = new QLabel(
        "Pick a profile, then tick which drivers / apps / tweaks to run on this PC. "
        "To add profiles or change their settings, edit the .json files in the profiles "
        "folder.", central);
    profHint->setWordWrap(true);
    profHint->setStyleSheet("color: #666;");
    root->addWidget(profHint);

    // Checklist tabs
    m_tabs = new ChecklistTabs(central);
    root->addWidget(m_tabs, 2);

    // Progress — aligned grid: right-justified labels, uniform-width bars.
    auto* progGroup = new QGroupBox("Progress", central);
    auto* pg = new QGridLayout(progGroup);
    pg->setColumnStretch(1, 1);
    auto rowFor = [&](int row, const char* label, QProgressBar*& bar) {
        auto* lbl = new QLabel(label, progGroup);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pg->addWidget(lbl, row, 0);
        bar = new QProgressBar(progGroup);
        bar->setRange(0, 100);
        bar->setMinimumWidth(320);
        pg->addWidget(bar, row, 1);
    };
    rowFor(0, "Drivers:", m_driverBar);
    rowFor(1, "Applications:", m_appBar);
    auto* taskLbl = new QLabel("Current task:", progGroup);
    taskLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pg->addWidget(taskLbl, 2, 0);
    m_currentTask = new QLabel("Idle", progGroup);
    pg->addWidget(m_currentTask, 2, 1);
    root->addWidget(progGroup);

    // Buttons
    auto* bRow = new QHBoxLayout;
    m_startBtn = new QPushButton("Start", central);
    m_cancelBtn = new QPushButton("Cancel", central);
    m_cancelBtn->setEnabled(false);
    m_saveReportBtn = new QPushButton("Save report…", central);
    m_saveReportBtn->setEnabled(false);
    bRow->addWidget(m_startBtn);
    bRow->addWidget(m_cancelBtn);
    bRow->addStretch();
    bRow->addWidget(m_saveReportBtn);
    root->addLayout(bRow);

    // Log + report
    m_log = new QPlainTextEdit(central);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(5000);
    root->addWidget(m_log, 2);
    m_report = new QPlainTextEdit(central);
    m_report->setReadOnly(true);
    m_report->setPlaceholderText("The provisioning report appears here when the run finishes.");
    root->addWidget(m_report, 1);

    setCentralWidget(central);

    connect(m_profileBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MainWindow::onProfilePicked);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancel);
    connect(m_saveReportBtn, &QPushButton::clicked, this, &MainWindow::onSaveReport);
    connect(&m_engine, &EngineController::logEvent, this, &MainWindow::onLogEvent);
    connect(&m_engine, &EngineController::stageChanged, this, &MainWindow::onStageChanged);
    connect(&m_engine, &EngineController::progress, this, &MainWindow::onProgress);
    connect(&m_engine, &EngineController::finished, this, &MainWindow::onFinished);

    populateProfiles();
    onProfilePicked(m_profileBox->currentIndex());
}

void MainWindow::populateProfiles() {
    m_profileBox->blockSignals(true);
    m_profileBox->clear();
    m_profileBox->addItem("(custom — start from blank)", QString());
    const QString exeDir = QCoreApplication::applicationDirPath();
    for (const QString& d : {exeDir + "/profiles", exeDir + "/../profiles",
                             exeDir + "/../../profiles"}) {
        QDir dir(d);
        if (!dir.exists()) continue;
        for (const QFileInfo& fi : dir.entryInfoList({"*.json"}, QDir::Files, QDir::Name))
            m_profileBox->addItem(fi.completeBaseName(), fi.absoluteFilePath());
        if (m_profileBox->count() > 1) break;
    }
    m_profileBox->blockSignals(false);
}

void MainWindow::onProfilePicked(int index) {
    const QString path = m_profileBox->itemData(index).toString();
    if (path.isEmpty()) {
        m_tabs->seed(blankProfile());
        return;
    }
    auto loaded = profiles::ProfileLoader::load(path.toStdString());
    if (std::holds_alternative<profiles::Profile>(loaded)) {
        m_tabs->seed(std::get<profiles::Profile>(loaded));
    } else {
        m_currentTask->setText("Profile error: " +
            QString::fromStdString(std::get<profiles::ProfileLoadError>(loaded).message));
        m_tabs->seed(blankProfile());
    }
}

void MainWindow::setRunning(bool running) {
    m_startBtn->setEnabled(!running);
    m_cancelBtn->setEnabled(running);
    m_profileBox->setEnabled(!running);
    m_dryRunBox->setEnabled(!running);
    m_tabs->setEnabledForRun(!running);
}

void MainWindow::onStart() {
    m_log->clear();
    m_report->clear();
    m_driverBar->setValue(0);
    m_appBar->setValue(0);
    m_saveReportBtn->setEnabled(false);

    provisioning::ProvisioningOptions o;
    o.profileObject = m_tabs->effectiveProfile();
    o.profile = o.profileObject->name;
    o.dryRun = m_dryRunBox->isChecked();
    o.skipDrivers = !o.profileObject->drivers.enabled;
    o.skipApps = o.profileObject->enabledApps().empty();

    setRunning(true);
    m_currentTask->setText("Starting…");
    m_engine.start(o);
}

void MainWindow::onCancel() {
    m_currentTask->setText("Cancelling…");
    m_engine.requestCancel();
}

void MainWindow::onLogEvent(QString isoTime, int severity, QString category, QString message,
                            int) {
    const QString ts = isoTime.mid(11, 8);
    QTextCharFormat fmt;
    fmt.setForeground(severityColor(severity));
    QTextCursor cur = m_log->textCursor();
    cur.movePosition(QTextCursor::End);
    cur.insertText(QString("[%1] %2  %3\n").arg(ts, category, message), fmt);
    m_log->setTextCursor(cur);
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
    const QString dst =
        QFileDialog::getSaveFileName(this, "Save report", "report.json", "JSON (*.json)");
    if (dst.isEmpty()) return;
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly)) return;
    const QJsonObject run = QJsonDocument::fromJson(in.readAll()).object();
    QFile out(dst);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        out.write(QJsonDocument(run.value("report").toObject()).toJson(QJsonDocument::Indented));
}

} // namespace shiftech::gui
