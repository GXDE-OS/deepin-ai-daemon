// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelhubwrapper.h"
#include "logcategory.h"

#include <QString>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QEventLoop>
#include <QFileInfo>
#include <QProcess>

#include <unistd.h>

AIDAEMON_USE_NAMESPACE

ModelhubWrapper::ModelhubWrapper(const QString &model, QObject *parent)
    : QObject(parent)
    , modelName(model)
{
    Q_ASSERT(!model.isEmpty());
    qCInfo(logDeployerWrapper) << "Creating ModelhubWrapper for model:" << model;
}

ModelhubWrapper::~ModelhubWrapper()
{
    qCInfo(logDeployerWrapper) << "ModelhubWrapper destructor called for model:" << modelName;
    
    if (!keepLive && pid > 0) {
        int rpid = modelStatus(modelName).value("pid").toInt();

        // start by me, to kill.
        if (rpid == pid) {
            qCInfo(logDeployerWrapper) << "Killing server for model:" << modelName << "pid:" << pid;
            system(QString("kill -3 %0").arg(pid).toStdString().c_str());
        } else {
            qCInfo(logDeployerWrapper) << "Server for model:" << modelName << "pid:" << pid << "was not launched by this instance";
        }
    }
}

bool ModelhubWrapper::isRunning()
{
    QString statFile = QString("/tmp/deepin-modelhub-%0/%1.state").arg(getuid()).arg(modelName);
    bool ret = QFileInfo::exists(statFile);
    qCDebug(logDeployerWrapper) << "Checking if model is running:" << modelName << "result:" << ret;
    return ret;
}

bool ModelhubWrapper::ensureRunning()
{
    qCInfo(logDeployerWrapper) << "Ensuring model is running:" << modelName;
    
    if (health()) {
        qCDebug(logDeployerWrapper) << "Model is already healthy and running:" << modelName;
        return true;
    }

    // check running by user
    QWriteLocker lk(&lock);
    {
        updateHost();
        if (!host.isEmpty() && port > 0) {
            qCInfo(logDeployerWrapper) << "Model server already running for:" << modelName << "host:" << host << "port:" << port;
            return true;
        }
    }

    const int idle = 180;
    int cur = QDateTime::currentSecsSinceEpoch();
    if (QDateTime::currentSecsSinceEpoch() - started < idle * 1000) {
        qCWarning(logDeployerWrapper) << "Cannot start model" << modelName << "- last attempt was too recent, started at:" << started;
        return false;
    }

    qCInfo(logDeployerWrapper) << "Starting modelhub server for model:" << modelName;

    QProcess process;
    process.setProgram("deepin-modelhub");
    process.setArguments({"--run", modelName, "--exit-idle", QString::number(idle)}); // 3分钟自动退出
    bool ok = process.startDetached(&pid);
    started = cur;
    if (!ok || pid < 1) {
        qCCritical(logDeployerWrapper) << "Failed to start modelhub server for model:" << modelName 
                                      << "program:" << process.program() 
                                      << "arguments:" << process.arguments();
        return false;
    }

    qCInfo(logDeployerWrapper) << "Modelhub server started for model:" << modelName << "with pid:" << pid;

    // wait server
    {
        const QString proc = QString("/proc/%0").arg(pid);
        int waitCount = 60 * 5; // 等1分钟加载模型
        qCInfo(logDeployerWrapper) << "Waiting for server to be ready for model:" << modelName;
        
        while (waitCount-- && QFileInfo::exists(proc)) {
            QThread::msleep(200);
            updateHost();
            if (!host.isEmpty() && port > 0) {
                qCInfo(logDeployerWrapper) << "Server ready for model:" << modelName << "pid:" << pid << "host:" << host << "port:" << port;
                return true;
            }
        }
        
        qCWarning(logDeployerWrapper) << "Timeout waiting for server to be ready for model:" << modelName;
    }

    return false;
}

bool ModelhubWrapper::health()
{
    QReadLocker lk(&lock);
    if (host.isEmpty() || port < 1) {
        qCDebug(logDeployerWrapper) << "Health check failed - invalid host/port for model:" << modelName << "host:" << host << "port:" << port;
        return false;
    }
    lk.unlock();

    QUrl url = urlPath("/health");
    qCDebug(logDeployerWrapper) << "Performing health check for model:" << modelName << "url:" << url.toString();

    QNetworkAccessManager manager;
    QNetworkRequest request(url);

    QNetworkReply *reply = manager.get(request);
    if (!reply) {
        qCWarning(logDeployerWrapper) << "Failed to create network request for health check:" << modelName;
        return false;
    }

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    
    bool isHealthy = reply->error() == QNetworkReply::NoError;
    if (isHealthy) {
        qCDebug(logDeployerWrapper) << "Health check passed for model:" << modelName;
    } else {
        qCWarning(logDeployerWrapper) << "Health check failed for model:" << modelName << "error:" << reply->errorString();
    }
    
    reply->deleteLater();
    return isHealthy;
}

void ModelhubWrapper::setKeepLive(bool live)
{
    qCInfo(logDeployerWrapper) << "Setting keep alive for model:" << modelName << "to:" << live;
    keepLive = live;
}

QString ModelhubWrapper::urlPath(const QString &api) const
{
    QReadLocker lk(&lock);
    return QString("http://%0:%1%2").arg(host).arg(port).arg(api);
}

bool ModelhubWrapper::isModelhubInstalled()
{
    qCDebug(logDeployerWrapper) << "Checking if modelhub is installed";
    
    QString out;
    if (openCmd("deepin-modelhub -v", out)) {
        qCInfo(logDeployerWrapper) << "Modelhub is installed, version output:" << out.trimmed();
        return true;
    }

    qCWarning(logDeployerWrapper) << "Modelhub is not installed or not accessible";
    return false;
}

bool ModelhubWrapper::isModelInstalled(const QString &model)
{
    qCDebug(logDeployerWrapper) << "Checking if model is installed:" << model;
    
    if (model.isEmpty()) {
        qCWarning(logDeployerWrapper) << "Cannot check installation status for empty model name";
        return false;
    }

    auto list = ModelhubWrapper::installedModels();
    bool isInstalled = list.contains(model, Qt::CaseInsensitive);
    qCDebug(logDeployerWrapper) << "Model installation check result for" << model << ":" << isInstalled;
    return isInstalled;
}

QVariantHash ModelhubWrapper::modelStatus(const QString &model)
{
    qCDebug(logDeployerWrapper) << "Getting model status for:" << model;
    
    if (model.isEmpty()) {
        qCWarning(logDeployerWrapper) << "Cannot get status for empty model name";
        return {};
    }

    QList<QVariantHash> infos = ModelhubWrapper::modelsStatus();
    for (auto mvh : infos) {
        QString name = mvh.value("model").toString();
        if (name.compare(model, Qt::CaseInsensitive) == 0){
            qCDebug(logDeployerWrapper) << "Found status for model:" << model << "pid:" << mvh.value("pid") << "port:" << mvh.value("port");
            return mvh;
        }
    }

    qCDebug(logDeployerWrapper) << "No status found for model:" << model;
    return {};
}

QList<QVariantHash> ModelhubWrapper::modelsStatus()
{
    qCDebug(logDeployerWrapper) << "Getting status for all models";
    
    QList<QVariantHash> ret;
    QString out;
    if (openCmd("deepin-modelhub --list server --info", out)) {
        auto vh = QJsonDocument::fromJson(out.toUtf8()).object().toVariantHash();
        auto ml = vh.value("serverinfo").value<QVariantList>();
        for (const QVariant &var: ml) {
            auto mvh = var.value<QVariantHash>();
            ret.append(mvh);
        }
        qCInfo(logDeployerWrapper) << "Retrieved status for" << ret.size() << "models";
    } else {
        qCWarning(logDeployerWrapper) << "Failed to get models status information";
    }

    return ret;
}

bool ModelhubWrapper::openCmd(const QString &cmd, QString &out)
{
    qCDebug(logDeployerWrapper) << "Executing command:" << cmd;
    
    FILE* pipe = popen(cmd.toStdString().c_str(), "r");
    if (!pipe) {
        qCCritical(logDeployerWrapper) << "Failed to execute command:" << cmd;
        return false;
    }

    char buffer[256] = {0};
    bool ok = fgets(buffer, sizeof(buffer), pipe) != nullptr;
    if (!ok) {
        pclose(pipe);
        qCWarning(logDeployerWrapper) << "No output from command:" << cmd;
        return false;
    }

    pclose(pipe);

    out = QString(buffer);
    qCDebug(logDeployerWrapper) << "Command executed successfully:" << cmd << "output length:" << out.length();
    return true;
}

QStringList ModelhubWrapper::installedModels()
{
    qCInfo(logDeployerWrapper) << "Getting list of installed models";
    
    QString out;
    QStringList modelList;
    if (ModelhubWrapper::openCmd("deepin-modelhub --list model", out)) {
        auto vh = QJsonDocument::fromJson(out.toUtf8()).object().toVariantHash();
        modelList = vh.value("model").toStringList();
        qCInfo(logDeployerWrapper) << "Found" << modelList.size() << "installed models:" << modelList;
    } else {
        qCWarning(logDeployerWrapper) << "Failed to get installed models list";
    }

    return modelList;
}

bool ModelhubWrapper::stopModel()
{
    qCInfo(logDeployerWrapper) << "Stopping model:" << modelName;
    
    QProcess process;
    process.start("deepin-modelhub --stop " + modelName);
    process.waitForFinished();

    QString errorOutput = process.readAllStandardError();
    bool success = !errorOutput.isEmpty();
    
    if (success) {
        qCInfo(logDeployerWrapper) << "Model stopped successfully:" << modelName;
    } else {
        qCWarning(logDeployerWrapper) << "Failed to stop model:" << modelName << "error:" << errorOutput;
    }

    return success;
}

void ModelhubWrapper::updateHost()
{
    qCDebug(logDeployerWrapper) << "Updating host information for model:" << modelName;
    
    auto vh = modelStatus(modelName);
    host = vh.value("host").toString();
    port = vh.value("port").toInt();
    
    qCDebug(logDeployerWrapper) << "Host information updated for model:" << modelName << "host:" << host << "port:" << port;
}
