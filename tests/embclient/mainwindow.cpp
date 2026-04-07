// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "testwidget.h"
#include "testmail.h"

#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
    , m_testWidget(nullptr)
    , m_testMail(nullptr)
{
    setupUI();
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    setWindowTitle("Embedding Platform Test");
    setMinimumSize(1000, 700);

    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    m_tabWidget = new QTabWidget();
    
    // 创建第一个tab - 原有的测试界面
    m_testWidget = new TestWidget();
    m_tabWidget->addTab(m_testWidget, "Embedding Test");
    
    // 创建第二个tab - 邮件测试界面
    m_testMail = new TestMail();
    m_tabWidget->addTab(m_testMail, "Mail Test");
    
    mainLayout->addWidget(m_tabWidget);
    
    setCentralWidget(centralWidget);
}