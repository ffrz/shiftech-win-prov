#include "EngineController.h"

#include <QThread>

using namespace shiftech::core::provisioning;

namespace shiftech::gui {

namespace {
// A QObject that does the work inside the worker thread.
class Worker : public QObject {
    Q_OBJECT
public:
    Worker(ProvisioningOptions opts, const std::atomic<bool>* cancel)
        : m_opts(std::move(opts)) {
        m_opts.cancelRequested = cancel;
    }

public slots:
    void run() {
        EventSink sink = [this](const ProvisioningEvent& e) {
            emit event(QString::fromStdString(e.timestamp), static_cast<int>(e.severity),
                       QString::fromStdString(e.category), QString::fromStdString(e.message),
                       e.progress);
            if (e.category == "pipeline" && e.message.rfind("stage: ", 0) == 0) {
                emit stage(QString::fromStdString(e.message.substr(7)));
            }
            if (e.progress >= 0) {
                emit prog(QString::fromStdString(e.category), e.progress);
            }
        };
        ProvisioningEngine engine(sink);
        ProvisioningResult r = engine.run(m_opts);
        emit done(QString::fromLatin1(toString(r.report.status)),
                  QString::fromStdString(r.report.toText()), r.runDir);
    }

signals:
    void event(QString, int, QString, QString, int);
    void stage(QString);
    void prog(QString, int);
    void done(QString, QString, QString);

private:
    ProvisioningOptions m_opts;
};
} // namespace

EngineController::EngineController(QObject* parent) : QObject(parent) {}

EngineController::~EngineController() {
    if (m_thread) {
        m_cancel = true;
        m_thread->quit();
        m_thread->wait(5000);
    }
}

void EngineController::start(const ProvisioningOptions& opts) {
    if (m_running) return;
    m_cancel = false;
    m_running = true;

    m_thread = new QThread(this);
    auto* worker = new Worker(opts, &m_cancel);
    worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, worker, &Worker::run);
    connect(worker, &Worker::event, this, &EngineController::logEvent);
    connect(worker, &Worker::stage, this, &EngineController::stageChanged);
    connect(worker, &Worker::prog, this, &EngineController::progress);
    connect(worker, &Worker::done, this,
            [this, worker](QString status, QString text, QString dir) {
                m_running = false;
                emit finished(status, text, dir);
                m_thread->quit();
                worker->deleteLater();
            });
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, this, [this]() { m_thread = nullptr; });

    emit started();
    m_thread->start();
}

void EngineController::requestCancel() {
    m_cancel = true;
}

} // namespace shiftech::gui

#include "EngineController.moc"
