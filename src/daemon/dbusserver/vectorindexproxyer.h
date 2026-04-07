#ifndef VECTORINDEXPROXYER_H
#define VECTORINDEXPROXYER_H

#include "aidaemon_global.h"
#include "api/vectordbinterface.h"

#include <QObject>
#include <QDBusMessage>
#include <QVariant>
#include <QSet>

AIDAEMON_BEGIN_NAMESPACE

class VectorIndexProxyer : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.VectorIndex")
public:
    explicit VectorIndexProxyer(QSharedPointer<AIDAEMON_VECTORDB_NAMESPACE::VectorDbInterface> ifs, QObject *parent = nullptr);
    ~VectorIndexProxyer();
    void start(QThread *dbusThread);
    static inline QString dependModel() {
        return QString("modelhub/BAAI-bge-large-zh-v1.5");
    }
public Q_SLOTS: // METHODS
    bool Create(const QString &appID, const QStringList &files);
    bool Delete(const QString &appID, const QStringList &files);
    QString DocFiles(const QString &appID);
    bool Enable();
    QString Search(const QString &appID, const QString &query, int topK);
    QString embeddingTexts(const QString &appID, const QStringList &texts);
    Q_DECL_DEPRECATED QString getAutoIndexStatus(const QString &appID);
    Q_DECL_DEPRECATED void saveAllIndex(const QString &appID);
    Q_DECL_DEPRECATED void setAutoIndex(const QString &appID, bool on);
Q_SIGNALS: // SIGNALS
    void IndexDeleted(const QString &appID, const QStringList &files);
    void IndexStatus(const QString &appID, const QStringList &files, int status);
private:
    QSharedPointer<AIDAEMON_VECTORDB_NAMESPACE::VectorDbInterface> interface;
    QSet<QString> m_whiteList;
};

class VectorIndexReply : public AIDAEMON_VECTORDB_NAMESPACE::VectorDbReply
{
public:
    explicit VectorIndexReply();
    ~VectorIndexReply() override;
    void replyError(int code, const QString &message) override;
    void replyString(const QString &message) override;
    void replyBool(bool ok) override;
    void waitForFinished(int timeout = -1);
    inline bool isError() {
        return error != 0;
    }

    template<typename T>
    inline T value() const {
        return content.value<T>();
    }
protected:
    std::atomic_bool finished = false;
    int error = QDBusError::TimedOut;
    QVariant content = QString("Time out.");
};

AIDAEMON_END_NAMESPACE

#endif // VECTORINDEXPROXYER_H
