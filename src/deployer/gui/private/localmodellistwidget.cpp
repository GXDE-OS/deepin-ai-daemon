#include "localmodellistwidget.h"
#include "themedlable.h"
#include "localmodelitem.h"
#include "gui/wrapper/modelhubwrapper.h"
#include "logcategory.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <DLabel>
#include <DFontSizeManager>
#include <DBackgroundGroup>
#include <DGuiApplicationHelper>

DWIDGET_USE_NAMESPACE
AIDAEMON_USE_NAMESPACE

LocalModelListWidget::LocalModelListWidget(DWidget *parent)
    : DWidget(parent)
{
    qCInfo(logDeployerUI) << "Initializing LocalModelListWidget";
    initUI();
    initConnect();
    onThemeTypeChanged();
    qCInfo(logDeployerUI) << "LocalModelListWidget initialization completed";
}

LocalModelListWidget::~LocalModelListWidget()
{
    qCInfo(logDeployerUI) << "LocalModelListWidget destructor called";
}

void LocalModelListWidget::initUI()
{
    qCDebug(logDeployerUI) << "Setting up UI for LocalModelListWidget";
    
    m_pModelListLayout = new QVBoxLayout(this);
    m_pModelListLayout->setContentsMargins(0, 0, 0, 0);
    m_pModelListLayout->setMargin(0);

    updateModelListItem();
    updateModelStatus();

    m_pModelListLayout->setSpacing(10);
    m_pModelListLayout->addStretch();
    
    qCDebug(logDeployerUI) << "UI setup completed for LocalModelListWidget";
}

void LocalModelListWidget::initConnect()
{
    qCDebug(logDeployerUI) << "Setting up signal connections for LocalModelListWidget";
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, &LocalModelListWidget::onThemeTypeChanged);
}

void LocalModelListWidget::onThemeTypeChanged()
{
    qCDebug(logDeployerUI) << "Theme type changed, updating background groups";
    
    for (DBackgroundGroup * modelWidget : m_backgroundGroupList) {
        DPalette pl = modelWidget->palette();
        pl.setBrush(DPalette::Base, DGuiApplicationHelper::instance()->applicationPalette().color(DPalette::ItemBackground));
        modelWidget->setPalette(pl);
    }
}

void LocalModelListWidget::updateModelListItem()
{
    qCInfo(logDeployerUI) << "Updating model list items";
    
    auto models = ModelhubWrapper::installedModels();
    qCInfo(logDeployerUI) << "Found" << models.size() << "installed models";

    for (const QString &modelName : models) {
        if (!modelName.isEmpty()) {
            qCDebug(logDeployerUI) << "Adding model to list:" << modelName;
            addItem(modelName);
        } else {
            qCWarning(logDeployerUI) << "Skipping empty model name";
        }
    }
    
    qCInfo(logDeployerUI) << "Model list items update completed";
}

void LocalModelListWidget::updateModelStatus()
{
    qCInfo(logDeployerUI) << "Updating model status information";
    
    QList<QVariantHash> infos = ModelhubWrapper::modelsStatus();
    qCInfo(logDeployerUI) << "Retrieved status for" << infos.size() << "models";
    
    for (auto mvh : infos) {
        QString name = mvh.value("model").toString();
        int port = mvh.value("port").toInt();

        qCDebug(logDeployerUI) << "Model status - name:" << name << "port:" << port;

        auto it = m_modelWidgetList.find(name);
        if (it != m_modelWidgetList.end()) {
            it.value()->updateLaunchStatus(true);
            qCDebug(logDeployerUI) << "Updated launch status for model:" << name;
        } else {
            qCWarning(logDeployerUI) << "Model widget not found for:" << name;
        }
    }
    
    qCInfo(logDeployerUI) << "Model status update completed";
}

void LocalModelListWidget::addItem(const QString &modelname)
{
    qCInfo(logDeployerUI) << "Adding new model item:" << modelname;
    
    if (modelname.isEmpty()) {
        qCWarning(logDeployerUI) << "Cannot add item with empty model name";
        return;
    }
    
    if (m_modelWidgetList.contains(modelname)) {
        qCWarning(logDeployerUI) << "Model item already exists:" << modelname;
        return;
    }

    LocalModelItem *item = new LocalModelItem(modelname, this);
    if (!item) {
        qCCritical(logDeployerUI) << "Failed to create LocalModelItem for:" << modelname;
        return;
    }
    
    item->setText(modelname, "");
    item->setMinimumHeight(60);
    item->setMaximumHeight(75);
    
    QHBoxLayout *bgLayout = new QHBoxLayout;
    bgLayout->setContentsMargins(0, 0, 0, 0);
    bgLayout->addWidget(item);

    DBackgroundGroup *modelWidget = new DBackgroundGroup(bgLayout, this);
    modelWidget->setContentsMargins(0, 0, 0, 0);
    m_modelWidgetList.insert(modelname, item);
    m_backgroundGroupList.push_back(modelWidget);

    m_pModelListLayout->addWidget(modelWidget);
    
    qCInfo(logDeployerUI) << "Successfully added model item:" << modelname;
}
