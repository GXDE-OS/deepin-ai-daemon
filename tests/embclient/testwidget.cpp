// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "testwidget.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QFileDialog>
#include <QVariant>

#include "EmbeddingPlatformInterface.h"

TestWidget::TestWidget(QWidget *parent)
    : QWidget(parent)
    , m_mainLayout(nullptr)
    , m_topLayout(nullptr)
    , m_buttonLayout(nullptr)
    , m_appIdEdit(nullptr)
    , m_openButton(nullptr)
    , m_modelIdCombo(nullptr)
    , m_addButton(nullptr)
    , m_deleteButton(nullptr)
    , m_refreshButton(nullptr)
    , m_buildButton(nullptr) // 初始化构建按钮
    , m_mainSplitter(nullptr)
    , m_listGroupBox(nullptr)
    , m_listLayout(nullptr)
    , m_list(nullptr)
    , m_detailsGroupBox(nullptr)
    , m_detailsLayout(nullptr)
    , m_detailsText(nullptr)
    , m_dbus(nullptr)
{
    setupUI();
}

TestWidget::~TestWidget()
{
}

void TestWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);

    // Top: App ID and open button
    m_topLayout = new QHBoxLayout();
    QLabel *appIdLabel = new QLabel("App ID:");
    m_appIdEdit = new QLineEdit();
    m_appIdEdit->setText("uos-ai");
    m_appIdEdit->setPlaceholderText("Enter application ID");
    m_openButton = new QPushButton("Connect DBus");
    m_topLayout->addWidget(appIdLabel);
    m_topLayout->addWidget(m_appIdEdit);
    m_topLayout->addWidget(m_openButton);
    m_topLayout->addStretch();
    m_mainLayout->addLayout(m_topLayout);

    // Model selection
    QHBoxLayout *modelLayout = new QHBoxLayout();
    QLabel *modelLabel = new QLabel("Model ID:");
    m_modelIdCombo = new QComboBox();
    m_modelIdCombo->setMinimumWidth(400);
    m_modelIdCombo->setEditable(true);
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(m_modelIdCombo);
    modelLayout->addStretch();
    m_mainLayout->addLayout(modelLayout);

    // Buttons
    m_buttonLayout = new QHBoxLayout();
    m_addButton = new QPushButton("Add Documents");
    m_deleteButton = new QPushButton("Delete Documents");
    m_refreshButton = new QPushButton("Refresh");
    m_buildButton = new QPushButton("Build Index"); // 创建构建按钮
    m_addButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
    m_refreshButton->setEnabled(false);
    m_buildButton->setEnabled(false); // 初始禁用构建按钮
    m_buttonLayout->addWidget(m_addButton);
    m_buttonLayout->addWidget(m_deleteButton);
    m_buttonLayout->addWidget(m_refreshButton);
    m_buttonLayout->addWidget(m_buildButton); // 添加构建按钮到布局
    m_buttonLayout->addStretch();
    m_mainLayout->addLayout(m_buttonLayout);

    // Search interface
    m_searchLayout = new QHBoxLayout();
    QLabel *searchLabel = new QLabel("Search:");
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("Enter search query");
    m_searchButton = new QPushButton("Search");
    m_searchButton->setEnabled(false);
    m_searchLayout->addWidget(searchLabel);
    m_searchLayout->addWidget(m_searchEdit);
    m_searchLayout->addWidget(m_searchButton);
    m_searchLayout->addStretch();
    m_mainLayout->addLayout(m_searchLayout);

    // Main area
    m_mainSplitter = new QSplitter(Qt::Horizontal);

    m_listGroupBox = new QGroupBox("Documents");
    m_listLayout = new QVBoxLayout(m_listGroupBox);
    m_list = new QListWidget();
    m_listLayout->addWidget(m_list);
    m_mainSplitter->addWidget(m_listGroupBox);

    m_detailsGroupBox = new QGroupBox("Details");
    m_detailsLayout = new QVBoxLayout(m_detailsGroupBox);
    m_detailsText = new QTextEdit();
    m_detailsText->setReadOnly(true);
    m_detailsLayout->addWidget(m_detailsText);
    m_mainSplitter->addWidget(m_detailsGroupBox);

    m_mainSplitter->setSizes({300, 500});
    m_mainLayout->addWidget(m_mainSplitter);

    connect(m_openButton, &QPushButton::clicked, this, &TestWidget::onOpen);
    connect(m_addButton, &QPushButton::clicked, this, &TestWidget::onAddDocuments);
    connect(m_deleteButton, &QPushButton::clicked, this, &TestWidget::onDeleteDocuments);
    connect(m_refreshButton, &QPushButton::clicked, this, &TestWidget::onRefreshDocuments);
    connect(m_buildButton, &QPushButton::clicked, this, &TestWidget::onBuildIndex); // 连接构建按钮
    connect(m_searchButton, &QPushButton::clicked, this, &TestWidget::onSearch); // 连接搜索按钮
    connect(m_modelIdCombo, &QComboBox::currentTextChanged, this, &TestWidget::onModelSelectionChanged);
    connect(m_list, &QListWidget::itemClicked, this, &TestWidget::onDocumentSelected);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        // 当选择文档时启用构建按钮
        m_buildButton->setEnabled(!m_list->selectedItems().isEmpty());
    });
}

void TestWidget::onOpen()
{
    if (m_dbus) return;

    m_dbus = new OrgDeepinAiDaemonEmbeddingPlatformInterface(
        "org.deepin.ai.daemon.EmbeddingPlatform",
        "/org/deepin/ai/daemon/EmbeddingPlatform",
        QDBusConnection::sessionBus(), this);

    if (!m_dbus->isValid()) {
        logError("Failed to connect to EmbeddingPlatform DBus interface");
        delete m_dbus;
        m_dbus = nullptr;
        return;
    }

    // Load models from DBus (JSON string)
    QDBusReply<QString> modelsJson = m_dbus->embeddingModels();
    if (!modelsJson.isValid()) {
        logError("embeddingModels call failed");
    } else {
        // Parse JSON array and extract model field
        m_modelIdCombo->clear();
        
        QJsonDocument doc = QJsonDocument::fromJson(modelsJson.value().toUtf8());
        auto root = doc.object().value("results");
        if (root.isArray()) {
            QJsonArray modelsArray = root.toArray();
            for (const QJsonValue &value : modelsArray) {
                if (value.isObject()) {
                    QJsonObject modelObj = value.toObject();
                    if (modelObj.contains("model")) {
                        QString modelId = modelObj["model"].toString();
                        m_modelIdCombo->addItem(modelId);
                    }
                }
            }
        } else {
            // Fallback: keep original behavior if not a JSON array
            m_modelIdCombo->addItem(modelsJson.value());
        }
    }

    m_openButton->setEnabled(false);
    m_addButton->setEnabled(true);
    m_deleteButton->setEnabled(true);
    m_refreshButton->setEnabled(true);
    // 构建按钮初始状态为禁用，等待选择文档
    m_buildButton->setEnabled(false);
    m_searchButton->setEnabled(true);

    refreshDocuments();
}

void TestWidget::onBuildIndex()
{
    // 构建索引的处理函数（保持原有实现）
    if (!m_dbus || m_list->selectedItems().isEmpty()) return;
    
    QListWidgetItem *item = m_list->selectedItems().first();
    int row = m_list->row(item);
    auto documentId = m_docs[row].value("id").toString();
    QDBusReply<QString> reply = m_dbus->buildIndex(m_appIdEdit->text(), documentId);
    if (reply.isValid()) {
        logMessage("Build index success: " + reply.value());
    } else {
        logError("Build index failed: " + reply.error().message());
    }
}

void TestWidget::onSearch()
{
    // 搜索处理函数（保持原有实现）
    if (!m_dbus) return;
    
    QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty()) {
        logWarning("Please enter a search query");
        return;
    }
    
    QDBusReply<QString> reply = m_dbus->search(m_appIdEdit->text(), query);
    if (reply.isValid()) {
        m_detailsText->setText(formatReply(reply.value()));
    } else {
        logError("Search failed: " + reply.error().message());
    }
}

QString TestWidget::formatReply(const QString &reply)
{
    return QJsonDocument(QJsonDocument::fromJson(reply.toUtf8()).object().value("results").toArray())
            .toJson(QJsonDocument::Indented);
}

void TestWidget::onAddDocuments()
{
    // 添加文档处理函数（保持原有实现）
    if (!m_dbus) return;
    
    QStringList files = QFileDialog::getOpenFileNames(this, "Select documents");
    if (files.isEmpty()) return;
    
    QVariantHash hash;
    hash.insert("model", m_modelIdCombo->currentText());
    auto params = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantHash(hash)).toJson());
    for (const QString &file : files) {
        QDBusReply<QString> reply = m_dbus->uploadDocuments(m_appIdEdit->text(), {file}, params);
        if (reply.isValid()) {
            logMessage("Added document: " + reply.value());
        } else {
            logError("Add document failed: " + reply.error().message());
        }
    }
    
    refreshDocuments();
}

void TestWidget::onDeleteDocuments()
{
    // 删除文档处理函数（保持原有实现）
    if (!m_dbus || m_list->selectedItems().isEmpty()) return;
    
    QListWidgetItem *item = m_list->selectedItems().first();
    int row = m_list->row(item);
    auto documentId = m_docs[row].value("id").toString();
    
    QDBusReply<QString> reply = m_dbus->deleteDocuments(m_appIdEdit->text(), {documentId});
    if (reply.isValid()) {
        logMessage("Deleted document: " + documentId);
        delete m_list->takeItem(row);
        m_docs.removeAt(row);
    } else {
        logError("Delete document failed: " + reply.error().message());
    }
}

void TestWidget::onModelSelectionChanged()
{
    // 模型选择改变处理函数（保持原有实现）
    if (m_dbus) {
        refreshDocuments();
    }
}

void TestWidget::onRefreshDocuments()
{
    // 刷新文档处理函数（保持原有实现）
    if (m_dbus) {
        refreshDocuments();
    }
}

void TestWidget::onDocumentSelected(QListWidgetItem *item)
{
    // 文档选择处理函数（保持原有实现）
    if (!m_dbus || !item) return;
    
    int row = m_list->row(item);
    auto documentId = m_docs[row].value("id").toString();
    QDBusReply<QString> reply = m_dbus->documentsInfo(m_appIdEdit->text(), {documentId});
    if (reply.isValid()) {
        m_detailsText->setText(formatReply(reply.value()));
    } else {
        m_detailsText->setText("Failed to get document info: " + reply.error().message());
    }
}

void TestWidget::refreshDocuments()
{
    // 刷新文档列表处理函数（保持原有实现）
    if (!m_dbus) return;
    
    QString modelId = m_modelIdCombo->currentText();
    QDBusReply<QString> reply = m_dbus->documentsInfo(m_appIdEdit->text(), {});
    
    m_list->clear();
    m_docs.clear();
    
    if (reply.isValid()) {
        QJsonDocument doc = QJsonDocument::fromJson(reply.value().toUtf8());
        auto root = doc.object().value("results");
        if (root.isArray()) {
            QJsonArray documentsArray = root.toArray();
            for (const QJsonValue &value : documentsArray) {
                if (value.isObject()) {
                    QJsonObject docObj = value.toObject();
                    if (docObj.contains("id")) {
                        m_docs.append(docObj.toVariantHash());
                        QString file = docObj["file_name"].toString();
                        m_list->addItem(file);
                    }
                }
            }
        } else {
            // Fallback: keep original behavior if not a JSON array
            m_list->addItem(reply.value());
        }
    } else {
        logError("Get documents failed: " + reply.error().message());
    }
}

void TestWidget::logMessage(const QString &message)
{
    // 日志消息处理函数（保持原有实现）
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    qInfo() << ("[" + timestamp + "] " + message);
}

void TestWidget::logError(const QString &error)
{
    // 错误日志处理函数（保持原有实现）
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    qCritical() << ("[" + timestamp + "] ERROR: " + error);
}

void TestWidget::logWarning(const QString &warning)
{
    // 警告日志处理函数（保持原有实现）
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    qWarning() << ("[" + timestamp + "] WARNING: " + warning);
}
