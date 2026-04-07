// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "testmail.h"

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
#include <QTextDocument>

#include "EmbeddingPlatformInterface.h"

TestMail::TestMail(QWidget *parent)
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

TestMail::~TestMail()
{
}

QList<TestMail::Mail> TestMail::exportMails()
{
    QList<TestMail::Mail> rets;
    QDBusInterface dbus("org.deepin.mail", "/org/deepin/mail", "org.deepin.mail");
    QMap<QString, QStringList> accs;
    {
        QDBusReply<QString> reply = dbus.call("GetAccounts");
        auto root = QJsonDocument::fromJson(reply.value().toUtf8()).object();
        auto jaccs = root.value("accounts").toArray();
        for (const QJsonValue &val : jaccs) {
            auto obj = val.toObject();
            QString id = obj.value("id").toString();
            if (id.isEmpty())
                continue;

            QStringList names;
            auto folders = obj.value("folders").toArray();
            for (const QJsonValue &v : folders) {
                auto folder = v.toObject().value("name").toString();
                names.append(folder);
            }
            accs.insert(id, names);
        }
    }

    {
        for (auto it = accs.begin(); it != accs.end(); ++it) {
            QString id = it.key();

            for (const QString &floder : it.value()) {
                QDBusReply<QString> reply = dbus.call("GetMails", id, floder, 99);
                QJsonObject root = QJsonDocument::fromJson(reply.value().toUtf8()).object();
                auto ja = root.value("mails").toArray();
                for (const QJsonValue &v : ja) {
                    Mail mail;
                    auto jo = v.toObject();
                    int mailID = jo.value("id").toInt();
                    QDBusReply<QString> strDet = dbus.call("GetMailDetail", id, floder, mailID);
                    QJsonObject jdet = QJsonDocument::fromJson(strDet.value().toUtf8()).object();
                    mail.subject = jdet["subject"].toString();
                    {
                        QTextDocument doc;
                        doc.setHtml(jdet["body"].toString());
                        mail.body = doc.toPlainText().trimmed();
                    }
                    mail.sender = jdet["from"].toString();
                    mail.receivers.append(jdet["to"].toString());
                    mail.date = jdet["date"].toString();
                    rets.append(mail);
                }
            }
        }
    }

    return rets;
}

// 在setupUI函数中添加邮件列表
void TestMail::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);

    // Top: App ID and open button
    m_topLayout = new QHBoxLayout();
    QLabel *appIdLabel = new QLabel("App ID:");
    m_appIdEdit = new QLineEdit();
    m_appIdEdit->setText("deepin-mail");
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
    m_addButton = new QPushButton("Upload");
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

    // 添加邮件列表
    m_mailGroupBox = new QGroupBox("Mails");
    m_mailLayout = new QVBoxLayout(m_mailGroupBox);
    m_mailList = new QListWidget();
    m_getMailButton = new QPushButton("Get Mails");
    m_mailLayout->addWidget(m_mailList);
    m_mailLayout->addWidget(m_getMailButton);
    m_mainSplitter->addWidget(m_mailGroupBox);

    m_listGroupBox = new QGroupBox("Documents");
    m_listLayout = new QVBoxLayout(m_listGroupBox);
    m_list = new QListWidget();
    m_listLayout->addWidget(m_list);
    m_mainSplitter->addWidget(m_listGroupBox);

    m_detailsGroupBox = new QGroupBox("Details");
    m_detailsGroupBox->setMinimumWidth(300);
    m_detailsLayout = new QVBoxLayout(m_detailsGroupBox);
    m_detailsText = new QTextEdit();
    m_detailsText->setReadOnly(true);
    m_detailsLayout->addWidget(m_detailsText);
    m_mainSplitter->addWidget(m_detailsGroupBox);

    m_mainLayout->addWidget(m_mainSplitter);

    connect(m_openButton, &QPushButton::clicked, this, &TestMail::onOpen);
        connect(m_addButton, &QPushButton::clicked, this, &TestMail::onUploadMails);
    connect(m_deleteButton, &QPushButton::clicked, this, &TestMail::onDeleteDocuments);
    connect(m_refreshButton, &QPushButton::clicked, this, &TestMail::onRefreshDocuments);
    connect(m_buildButton, &QPushButton::clicked, this, &TestMail::onBuildIndex); // 连接构建按钮
    connect(m_searchButton, &QPushButton::clicked, this, &TestMail::onSearch); // 连接搜索按钮
    connect(m_modelIdCombo, &QComboBox::currentTextChanged, this, &TestMail::onModelSelectionChanged);
    connect(m_list, &QListWidget::itemClicked, this, &TestMail::onDocumentSelected);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
        // 当选择文档时启用构建按钮
        m_buildButton->setEnabled(!m_list->selectedItems().isEmpty());
    });
    connect(m_getMailButton, &QPushButton::clicked, this, &TestMail::onGetMails);
    connect(m_mailList, &QListWidget::itemClicked, this, &TestMail::onMailSelected);
    connect(m_mailList, &QListWidget::itemSelectionChanged, this, [this]() {
        // 当邮件列表有选中元素时启用upload按钮
        m_addButton->setEnabled(!m_mailList->selectedItems().isEmpty());
    });

}

void TestMail::onOpen()
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

void TestMail::onBuildIndex()
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

void TestMail::onSearch()
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

void TestMail::onDeleteDocuments()
{
    // 删除文档处理函数（保持原有实现）
    if (!m_dbus || m_list->selectedItems().isEmpty()) return;

    QListWidgetItem *item = m_list->selectedItems().first();
    int row = m_list->row(item);
    auto documentId = m_docs[row].value("id").toString();

    QDBusReply<QString> reply = m_dbus->deleteDocuments(m_appIdEdit->text(), {documentId});
    if (reply.isValid()) {
        logMessage("Deleted document: " + documentId);
        delete m_list->takeItem(m_list->row(item));
        m_docs.removeAt(row);
    } else {
        logError("Delete document failed: " + reply.error().message());
    }
}

void TestMail::onModelSelectionChanged()
{
    // 模型选择改变处理函数（保持原有实现）
    if (m_dbus) {
        refreshDocuments();
    }
}

void TestMail::onRefreshDocuments()
{
    // 刷新文档处理函数（保持原有实现）
    if (m_dbus) {
        refreshDocuments();
    }
}

void TestMail::onDocumentSelected(QListWidgetItem *item)
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

void TestMail::refreshDocuments()
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

void TestMail::logMessage(const QString &message)
{
    // 日志消息处理函数（保持原有实现）
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    qInfo() << ("[" + timestamp + "] " + message);
}

void TestMail::logError(const QString &error)
{
    // 错误日志处理函数（保持原有实现）
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    qCritical() << ("[" + timestamp + "] ERROR: " + error);
}

void TestMail::logWarning(const QString &warning)
{
    // 警告日志处理函数（保持原有实现）
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    qWarning() << ("[" + timestamp + "] WARNING: " + warning);
}

// 添加获取邮件处理函数
void TestMail::onGetMails()
{
    m_mailList->clear();
    m_mails = exportMails();
    for (const Mail &mail : m_mails) {
        m_mailList->addItem(mail.subject);
    }
}

void TestMail::onMailSelected(QListWidgetItem *item)
{
    if (!item) return;

    int row = m_mailList->row(item);
    if (row >= 0 && row < m_mails.size()) {
        const Mail &mail = m_mails[row];
        QString details = QString("Subject: %1\nFrom: %2\nTo: %3\nDate: %4\nBody:\n%5")
                          .arg(mail.subject)
                          .arg(mail.sender)
                          .arg(mail.receivers.join(", "))
                          .arg(mail.date)
                          .arg(mail.body);
        m_detailsText->setText(details);
    }
}


void TestMail::onUploadMails()
{
    // 上传选中邮件处理函数
    if (!m_dbus || m_mailList->selectedItems().isEmpty()) return;

    // 获取选中的邮件
    QListWidgetItem *item = m_mailList->selectedItems().first();
    int row = m_mailList->row(item);
    if (row < 0 || row >= m_mails.size()) return;

    const Mail &mail = m_mails[row];

    // 过滤邮件主题，去除不支持的字符
    QString fileName = mail.subject;
    fileName.replace(QRegularExpression("[\\/\\:*?\"<>|]"), "_"); // 替换不支持的字符为下划线

    // 添加发送时间
    QString sendTime = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    fileName = QString("%1_%2.txt").arg(fileName).arg(sendTime);

    // 创建临时文件路径
    QString tempDir = "/tmp";
    QString tempFilePath = QString("%1/%2").arg(tempDir).arg(fileName);

    // 将邮件内容写入临时文件
    QFile tempFile(tempFilePath);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logError("Failed to create temporary file: " + tempFilePath);
        return;
    }

    QTextStream out(&tempFile);
    out << "Subject: " << mail.subject << "\n";
    out << "From: " << mail.sender << "\n";
    out << "To: " << mail.receivers.join(", ") << "\n";
    out << "Date: " << mail.date << "\n";
    out << "Body:\n" << mail.body;
    tempFile.close();

    // 调用DBus接口上传文件
    QVariantHash hash;
    hash.insert("model", m_modelIdCombo->currentText());
    auto params = QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantHash(hash)).toJson());
    QDBusReply<QString> reply = m_dbus->uploadDocuments(m_appIdEdit->text(), {tempFilePath}, params);
    if (reply.isValid()) {
        logMessage("Uploaded mail: " + reply.value());
    } else {
        logError("Upload mail failed: " + reply.error().message());
    }

    // 刷新文档列表
    refreshDocuments();
}

QString TestMail::formatReply(const QString &reply)
{
    return QJsonDocument(QJsonDocument::fromJson(reply.toUtf8()).object().value("results").toArray())
            .toJson(QJsonDocument::Indented);
}
