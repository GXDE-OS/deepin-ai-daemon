// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SESSIONMANAGERDBUS_H
#define SESSIONMANAGERDBUS_H

#include "aidaemon_global.h"
#include "common/aitypes.h"

#include <QObject>
#include <QDBusContext>
#include <QMutex>
#include <QMultiMap>

AIDAEMON_BEGIN_NAMESPACE

class SessionManager;

class SessionManagerDBus : public QObject, public QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.ai.daemon.SessionManager")
    
public:
    explicit SessionManagerDBus(SessionManager* sessionManager, QObject *parent = nullptr);
    ~SessionManagerDBus();
    
public slots:
    QString CreateSession(const QString &type);
    void DestroySession(const QString &sessionId);
    QStringList GetAllSessions();
    
private slots:
    void onDBusOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);
    void onSessionDestroyed(const QString& sessionId);
    
private:
    // Helper methods
    AITypes::ServiceType stringToServiceType(const QString& typeStr) const;
    void cleanupClientSessions(const QString& clientId);
    
    // Core components
    SessionManager* sessionManager = nullptr;
    
    // Client tracking for cleanup on disconnect
    QMultiMap<QString, QString> dbusClientSessions; // dbusService -> sessionIds
    mutable QMutex clientMutex;
};

AIDAEMON_END_NAMESPACE

#endif // SESSIONMANAGERDBUS_H 
