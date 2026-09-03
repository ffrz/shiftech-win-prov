#pragma once

#include "../core/provisioning/ProvisioningEngine.h"
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

class QThread;

namespace shiftech::gui {

// Runs a ProvisioningEngine on a worker thread and re-emits its std::function events as
// Qt signals on the GUI thread. All engine/pipeline logic stays in core; this class only
// marshals threads and signals.
class EngineController : public QObject {
    Q_OBJECT
public:
    explicit EngineController(QObject* parent = nullptr);
    ~EngineController() override;

    bool running() const { return m_running; }

public slots:
    void start(const shiftech::core::provisioning::ProvisioningOptions& opts);
    void requestCancel();

signals:
    void started();
    void logEvent(QString isoTime, int severity, QString category, QString message, int progress);
    void stageChanged(QString stage);
    void progress(QString stage, int percent);
    void finished(QString status, QString reportText, QString runDir);

private:
    QThread* m_thread = nullptr;
    std::atomic<bool> m_cancel{false};
    bool m_running = false;
};

} // namespace shiftech::gui
