#include "localmodelitem.h"
#include "gui/wrapper/modelhubwrapper.h"
#include "logcategory.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
//#include <QtConcurrent>
#include <QResizeEvent>

#include <DFontSizeManager>

static constexpr int TIMERBEGIN = 720;//5秒轮询安装和更新状态60分钟

AIDAEMON_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

LocalModelItem::LocalModelItem(const QString &modelName, DWidget *parent)
    : m_modelName(modelName),
      DWidget(parent)
{
    qCInfo(logDeployerUI) << "Creating LocalModelItem for model:" << modelName;
    
    if (modelName.isEmpty()) {
        qCWarning(logDeployerUI) << "Model name is empty";
    }
    
    initUI();
    initConnect();
    m_modelhub = new ModelhubWrapper(m_modelName, this);
    
    qCInfo(logDeployerUI) << "LocalModelItem created successfully for model:" << modelName;
}

LocalModelItem::~LocalModelItem()
{
    qCInfo(logDeployerUI) << "LocalModelItem destructor called for model:" << m_modelName;
}

void LocalModelItem::initUI()
{
    qCDebug(logDeployerUI) << "Initializing UI for LocalModelItem:" << m_modelName;
    
    m_pLabelTheme = new DLabel;
    DFontSizeManager::instance()->bind(m_pLabelTheme, DFontSizeManager::T6, QFont::Medium);
    m_pLabelSummary = new DLabel;
    DFontSizeManager::instance()->bind(m_pLabelSummary, DFontSizeManager::T8, QFont::Normal);
    m_pLabelSummary->setElideMode(Qt::ElideRight);

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);
    textLayout->addWidget(m_pLabelTheme, 0, Qt::AlignLeft);
    textLayout->addWidget(m_pLabelSummary, 0, Qt::AlignLeft);

    m_pBtnInstall = new DSuggestButton(tr("Install"));
    m_pBtnInstall->setFixedHeight(30);
    m_pBtnInstall->setMinimumWidth(70);
    m_pBtnInstall->setMaximumWidth(80);
    m_pBtnInstall->hide();

    m_pBtnLaunch = new DSuggestButton(tr("Launch"));
    m_pBtnLaunch->setFixedHeight(30);
    m_pBtnLaunch->setMinimumWidth(70);
    m_pBtnLaunch->setMaximumWidth(90);
    m_pBtnLaunch->setDisabled(true);
//    m_pBtnUpdate->hide();

    m_pBtnUninstall = new DPushButton(tr("Uninstall"));
    m_pBtnUninstall->setFixedHeight(30);
    m_pBtnUninstall->setMinimumWidth(70);
    m_pBtnUninstall->setMaximumWidth(100);
    m_pBtnUninstall->hide();

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->addLayout(textLayout);
    mainLayout->addStretch();
    mainLayout->addWidget(m_pBtnInstall, 0, Qt::AlignVCenter);
    mainLayout->addWidget(m_pBtnUninstall, 0, Qt::AlignVCenter);
    mainLayout->addWidget(m_pBtnLaunch, 0, Qt::AlignVCenter);

    setLayout(mainLayout);

    onInstall(); // 测试，默认已安装
    
    qCDebug(logDeployerUI) << "UI initialization completed for LocalModelItem:" << m_modelName;
}

void LocalModelItem::initConnect()
{
    qCDebug(logDeployerUI) << "Setting up signal connections for LocalModelItem:" << m_modelName;
    
    connect(m_pBtnInstall, &DSuggestButton::clicked, this, &LocalModelItem::onInstall);
    connect(m_pBtnLaunch, &DSuggestButton::clicked, this, &LocalModelItem::onLaunchModel);
    connect(m_pBtnUninstall, &DSuggestButton::clicked, this, &LocalModelItem::onUninstall);
}

void LocalModelItem::setText(const QString &theme, const QString &summary)
{
    qCDebug(logDeployerUI) << "Setting text for model:" << m_modelName << "theme:" << theme;
    
    m_pLabelTheme->setText(theme);
    m_pLabelSummary->setText(summary);
}

void LocalModelItem::onInstall()
{
    qCInfo(logDeployerUI) << "Installing model:" << m_modelName;
    
    // 安装模型
    m_pBtnLaunch->setDisabled(false);
    m_pBtnInstall->hide();
    m_pBtnUninstall->show();
    
    qCInfo(logDeployerUI) << "Model install status updated for:" << m_modelName;
}

void LocalModelItem::onLaunchModel()
{
    qCInfo(logDeployerUI) << "Launch button clicked for model:" << m_modelName << "current status:" << (m_isLaunch ? "running" : "stopped");
    
    // 更新状态为启动中
    if (!m_isLaunch) {
        m_pBtnLaunch->setText(tr("Launching"));
        qCInfo(logDeployerUI) << "Setting model status to launching:" << m_modelName;
    }

    m_isLaunch = !m_isLaunch;
    if (m_isLaunch) {
        qCInfo(logDeployerUI) << "Attempting to start model:" << m_modelName;
        if (m_modelhub->ensureRunning()) {
            updateLaunchStatus(true);
            qCInfo(logDeployerUI) << "Model started successfully:" << m_modelName;
        } else {
            qCWarning(logDeployerUI) << "Failed to start model:" << m_modelName;
            m_isLaunch = false;
            updateLaunchStatus(false);
        }
    } else {
        qCInfo(logDeployerUI) << "Attempting to stop model:" << m_modelName;
        if (m_modelhub->stopModel()) {
            updateLaunchStatus(false);
            qCInfo(logDeployerUI) << "Model stopped successfully:" << m_modelName;
        } else {
            qCWarning(logDeployerUI) << "Failed to stop model:" << m_modelName;
        }
    }
}

void LocalModelItem::onUninstall()
{
    qCInfo(logDeployerUI) << "Uninstalling model:" << m_modelName;
    
    // 卸载模型
    if (m_isLaunch) {
        qCInfo(logDeployerUI) << "Stopping model before uninstall:" << m_modelName;
        onLaunchModel(); // 先暂停模型再卸载
    }

    m_pBtnLaunch->setDisabled(true);
    m_pBtnLaunch->setText(tr("Launch"));
    m_pBtnInstall->show();
    m_pBtnUninstall->hide();
    
    qCInfo(logDeployerUI) << "Model uninstall status updated for:" << m_modelName;
}

void LocalModelItem::updateLaunchStatus(bool isLaunch)
{
    qCDebug(logDeployerUI) << "Updating launch status for model:" << m_modelName << "to:" << (isLaunch ? "running" : "stopped");
    
    m_isLaunch = isLaunch;
    if (!m_pBtnLaunch->isEnabled()) {
        qCDebug(logDeployerUI) << "Launch button is disabled, skipping status update for:" << m_modelName;
        return;
    }
    
    if (m_isLaunch) {
        m_pBtnLaunch->setText(tr("Stop"));
        qCInfo(logDeployerUI) << "Model launch status updated to running:" << m_modelName;
    } else {
        m_pBtnInstall->hide();
        m_pBtnLaunch->setText(tr("Launch"));
        qCInfo(logDeployerUI) << "Model launch status updated to stopped:" << m_modelName;
    }
}
