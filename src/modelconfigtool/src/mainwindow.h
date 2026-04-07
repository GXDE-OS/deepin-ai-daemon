// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "configmanager.h"
#include "modelconfigdialog.h"
#include "modelitem.h"

#include <QFileDialog>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFrame>
#include <QMessageBox>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openConfigFile();
    void saveConfigFile();
    void addModel(const QString &modelType);
    void editModel(const QString &modelType, int index);
    void deleteModel(const QString &modelType, int index);
    void setAsDefaultModel(const QString &modelType, int index);

private:
    void setupUI();
    void setupMenuBar();
    void createModelTypeSection(const QString &modelType, const QString &displayName, const QString &description);
    void refreshModelTypeSection(const QString &modelType);
    void applyStyles();
    QString getModelTypeDisplayName(const QString &modelType) const;
    QString getModelTypeDescription(const QString &modelType) const;
    QString getModelTypeIcon(const QString &modelType) const;

    ConfigManager *configManager = nullptr;
    QString currentConfigFile;
    
    // UI components
    QWidget *centralWidget = nullptr;
    QVBoxLayout *mainLayout = nullptr;
    QScrollArea *scrollArea = nullptr;
    QWidget *scrollContent = nullptr;
    QVBoxLayout *scrollLayout = nullptr;
    
    // Model type sections
    QMap<QString, QGroupBox*> modelTypeSections;
    QMap<QString, QVBoxLayout*> modelTypeLayouts;
    QMap<QString, QPushButton*> addButtons;
};

#endif // MAINWINDOW_H
