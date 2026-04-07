#ifndef VECTORDBREPLY_H
#define VECTORDBREPLY_H

#include "aidaemon_global.h"

#include <QString>
#include <QSharedPointer>

AIDAEMON_VECTORDB_BEGIN_NAMESPACE

class VectorDbReply
{
public:
    explicit VectorDbReply();
    virtual ~VectorDbReply();

    virtual void replyError(int code, const QString &message);
    virtual void replyString(const QString &message);
    virtual void replyBool(bool ok);

};

typedef QSharedPointer<VectorDbReply> VectorDbReplyPtr;

AIDAEMON_VECTORDB_END_NAMESPACE

#endif // VECTORDBREPLY_H
