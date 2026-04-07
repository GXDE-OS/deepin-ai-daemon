#include "deployerwidget.h"
#include "private/themedlable.h"
#include "logcategory.h"

#include <DStackedWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <DPushButton>
#include <DLabel>
#include <DListView>
#include <DTitlebar>
#include <DFrame>

DWIDGET_USE_NAMESPACE
AIDAEMON_USE_NAMESPACE

DeployerWidget::DeployerWidget(QWidget *parent)
    : DMainWindow(parent)
{
    qCInfo(logDeployerUI) << "Initializing DeployerWidget";
    initUI();
    qCInfo(logDeployerUI) << "DeployerWidget initialization completed";
}

void DeployerWidget::initUI()
{
    qCDebug(logDeployerUI) << "Setting up UI components";
    
    setFixedWidth(600);

    titlebar()->setMenuVisible(false);
    titlebar()->setTitle(tr("Model Management Interface"));

    DWidget *mainWidget = new DWidget(this);

    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->setContentsMargins(10, 10, 10, 10);

    DFrame *frame = new DFrame(this);
    QVBoxLayout *frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(0, 0, 0, 0);
    frame->setLineWidth(0);

    DWidget *scrollWidget = new DWidget(this);
    scrollWidget->setContentsMargins(20, 10, 20, 10);
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

    m_pScrollArea = new QScrollArea();
    m_pScrollArea->setWidgetResizable(true);
    m_pScrollArea->setWidget(scrollWidget);
    m_pScrollArea->setFrameShape(QFrame::NoFrame);
    m_pScrollArea->setWindowFlags(Qt::FramelessWindowHint);
    m_pScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
    m_pScrollArea->setContentsMargins(0, 0, 0, 0);
    m_pScrollArea->setLineWidth(0);
    m_pScrollArea->setAttribute(Qt::WA_TranslucentBackground);
    m_pScrollArea->installEventFilter(this);

    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(30);

    scrollLayout->addWidget(initModelListWidget());
    scrollLayout->addStretch();

    frameLayout->addWidget(m_pScrollArea);
    rightLayout->addWidget(frame);

    mainLayout->addLayout(rightLayout);

    setCentralWidget(mainWidget);
    
    qCDebug(logDeployerUI) << "UI components setup completed";
}

DeployerWidget::~DeployerWidget()
{
    qCInfo(logDeployerUI) << "DeployerWidget destructor called";
}

DWidget *DeployerWidget::initModelListWidget()
{
    qCDebug(logDeployerUI) << "Initializing model list widget";
    
    DWidget *modelConfigWidget = new DWidget(this);
    modelConfigWidget->setContentsMargins(0, 0, 0, 0);
    QVBoxLayout *modelConfigLayout = new QVBoxLayout(modelConfigWidget);
    modelConfigLayout->setContentsMargins(0, 0, 0, 0);

    ThemedLable *label = new ThemedLable(tr("Model List"));
    DFontSizeManager::instance()->bind(label, DFontSizeManager::T5, QFont::Bold);
    modelConfigWidget->setProperty("title", label->text());
    modelConfigWidget->setProperty("level", 1);

    m_pLocalModelListWidget = new LocalModelListWidget(this);

    modelConfigLayout->addWidget(label);
    modelConfigLayout->addWidget(m_pLocalModelListWidget);

    qCInfo(logDeployerUI) << "Model list widget initialized successfully";
    return modelConfigWidget;
}
