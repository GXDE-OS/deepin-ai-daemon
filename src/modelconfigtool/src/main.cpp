// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"

#include <QApplication>
#include <QStyleFactory>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("deepin-ai-modelconfig");
    app.setApplicationDisplayName("Deepin AI Model Configuration Tool");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("Deepin");
    app.setOrganizationDomain("deepin.org");
    
    // Apply modern style
    app.setStyle(QStyleFactory::create("Fusion"));
    
    // Set global font
    QFont font = app.font();
    font.setFamily("Noto Sans CJK SC");
    font.setPointSize(10);
    app.setFont(font);
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
