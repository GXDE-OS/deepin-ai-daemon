#ifndef LOCALMODELITEM_H
#define LOCALMODELITEM_H
#include "aidaemon_global.h"
#include "gui/wrapper/modelhubwrapper.h"

#include <QProcess>
#include <QDateTime>

#include <DWidget>
#include <DBackgroundGroup>
#include <DLabel>
#include <DSuggestButton>

AIDAEMON_BEGIN_NAMESPACE
class LocalModelItem: public DTK_WIDGET_NAMESPACE::DWidget
{
    Q_OBJECT

public:
    explicit LocalModelItem(const QString &modelName, DTK_WIDGET_NAMESPACE::DWidget *parent = nullptr);
    ~LocalModelItem();

    void updateLaunchStatus(bool isLaunch);
    void setText(const QString &theme, const QString &summary);

private:
    void initUI();
    void initConnect();

private slots:
    void onInstall();
    void onLaunchModel();
    void onUninstall();


private:
    DTK_WIDGET_NAMESPACE::DLabel *m_pLabelTheme = nullptr;
    DTK_WIDGET_NAMESPACE::DLabel *m_pLabelSummary = nullptr;
    DTK_WIDGET_NAMESPACE::DSuggestButton *m_pBtnInstall = nullptr;
    DTK_WIDGET_NAMESPACE::DSuggestButton *m_pBtnLaunch = nullptr;
    DTK_WIDGET_NAMESPACE::DPushButton *m_pBtnUninstall = nullptr;

    QString host;
    QString m_modelName;
    int pid;
    int port;
    QDateTime startTime;

    bool m_isLaunch = false;

    ModelhubWrapper *m_modelhub = nullptr;
};
AIDAEMON_END_NAMESPACE

#endif // LOCALMODELITEM_H
