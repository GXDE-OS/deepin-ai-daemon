// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TESTMAIL_H
#define TESTMAIL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QTextEdit>
#include <QLabel>
#include <QGroupBox>
#include <QSplitter>

class OrgDeepinAiDaemonEmbeddingPlatformInterface;

class TestMail : public QWidget
{
    Q_OBJECT

    struct Mail{
        QString subject;
        QString body;
        QString sender;
        QStringList receivers;
        QString date;
    };

public:
    explicit TestMail(QWidget *parent = nullptr);
    ~TestMail();


private slots:
    QList<Mail> exportMails();
    void onOpen();
    void onDeleteDocuments();
    void onModelSelectionChanged();
    void onRefreshDocuments();
    void onDocumentSelected(QListWidgetItem *item);
    void onBuildIndex(); // 添加构建按钮的槽函数
    void onSearch(); // 添加搜索按钮的槽函数
    void onGetMails();
    void onMailSelected(QListWidgetItem *item);
    void onUploadMails();
    QString formatReply(const QString &reply);

private:
    void setupUI();
    void refreshDocuments();
    void logMessage(const QString &message);
    void logError(const QString &error);
    void logWarning(const QString &warning);

    // UI components
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_topLayout;
    QHBoxLayout *m_buttonLayout;
    QLineEdit *m_appIdEdit;
    QPushButton *m_openButton;
    QComboBox *m_modelIdCombo;
    QPushButton *m_addButton;
    QPushButton *m_deleteButton;
    QPushButton *m_refreshButton;
    QPushButton *m_buildButton; // 添加构建按钮
    QHBoxLayout *m_searchLayout; // 添加搜索布局
    QLineEdit *m_searchEdit; // 添加搜索输入框
    QPushButton *m_searchButton; // 添加搜索按钮
    QSplitter *m_mainSplitter;
    QGroupBox *m_listGroupBox;
    QVBoxLayout *m_listLayout;
    QListWidget *m_list;
    QList<QVariantHash> m_docs;
    QGroupBox *m_detailsGroupBox;
    QVBoxLayout *m_detailsLayout;
    QTextEdit *m_detailsText;

    QGroupBox *m_mailGroupBox;
    QVBoxLayout *m_mailLayout;
    QListWidget *m_mailList;
    QList<Mail> m_mails;
    QPushButton *m_getMailButton;

    // DBus proxy
    OrgDeepinAiDaemonEmbeddingPlatformInterface *m_dbus;

};

#endif // TESTMAIL_H
