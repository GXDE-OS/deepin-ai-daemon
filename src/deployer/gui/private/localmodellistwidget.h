#ifndef LOCALMODELLISTWIDGET_H
#define LOCALMODELLISTWIDGET_H
#include "aidaemon_global.h"

#include <QProcess>
#include <QTimer>
#include <QVBoxLayout>

#include <DWidget>
#include <DBackgroundGroup>

AIDAEMON_BEGIN_NAMESPACE
class ThemedLable;
class LocalModelItem;
class LocalModelListWidget: public DTK_WIDGET_NAMESPACE::DWidget
{
    Q_OBJECT

public:
    explicit LocalModelListWidget(DTK_WIDGET_NAMESPACE::DWidget *parent = nullptr);
    ~LocalModelListWidget();

private slots:
    void onThemeTypeChanged();
    void updateModelListItem();

private:
    void initUI();
    void initConnect();
    void updateModelStatus();
    void addItem(const QString &modelname);

    DTK_WIDGET_NAMESPACE::DBackgroundGroup *embeddingPluginsWidget();

private:
    QMap<QString, LocalModelItem *> m_modelWidgetList;
    QList<DTK_WIDGET_NAMESPACE::DBackgroundGroup *> m_backgroundGroupList;

    QVBoxLayout *m_pModelListLayout = nullptr;
};
AIDAEMON_END_NAMESPACE
#endif // LOCALMODELLISTWIDGET_H
