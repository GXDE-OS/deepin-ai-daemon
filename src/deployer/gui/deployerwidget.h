#ifndef DEPLOYERWIDGET_H
#define DEPLOYERWIDGET_H
#include "aidaemon_global.h"
#include "private/localmodellistwidget.h"

#include <QScrollArea>

#include <DMainWindow>
#include <DWidget>

AIDAEMON_BEGIN_NAMESPACE
class DeployerWidget : public DTK_WIDGET_NAMESPACE::DMainWindow
{
    Q_OBJECT

public:
    DeployerWidget(QWidget *parent = nullptr);
    ~DeployerWidget();

private:
    void initUI();

    DTK_WIDGET_NAMESPACE::DWidget *initModelListWidget();

    QScrollArea *m_pScrollArea = nullptr;
    LocalModelListWidget *m_pLocalModelListWidget = nullptr;
};
AIDAEMON_END_NAMESPACE

#endif // DEPLOYERWIDGET_H
