#ifndef MVS_RUNNER_H
#define MVS_RUNNER_H

#include <QObject>
#include <QProcess>

class MvsRunner : public QObject
{
    Q_OBJECT
public:
    explicit MvsRunner(QObject* parent = nullptr);
    ~MvsRunner();

public slots:
    void startMvs(const QString& imagesPath,
                  const QString& workspacePath);
    void stop();

signals:
    void log(const QString& msg);
    void error(const QString& msg);
    void finished(const QString& fusedPlyPath);

private:
    bool runStep(const QStringList& args);

private:
    QProcess* m_process = nullptr;
    bool m_abort = false;
};

#endif // MVS_RUNNER_H
