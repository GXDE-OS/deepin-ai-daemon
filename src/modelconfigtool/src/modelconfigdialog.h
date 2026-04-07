// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELCONFIGDIALOG_H
#define MODELCONFIGDIALOG_H

#include "configmanager.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QFrame>
#include <QSpacerItem>

class ModelConfigDialog : public QDialog
{
    Q_OBJECT

public:
    // Constructor for adding new model
    explicit ModelConfigDialog(ConfigManager *configManager, const QString &modelType, QWidget *parent = nullptr);
    
    // Constructor for editing existing model
    explicit ModelConfigDialog(ConfigManager *configManager, const QString &modelType, const ModelConfig &config, QWidget *parent = nullptr);
    
    ModelConfig getModelConfig() const;

private slots:
    void onProviderChanged();
    void onOkClicked();
    void onCancelClicked();

private:
    void setupUI();
    void setupProviderSelection();
    void setupParameterInputs();
    void populateParameters();
    void applyStyles();
    void loadConfigData();
    void adjustDialogSize();
    QString getProviderDisplayName(const QString &provider) const;
    
    ConfigManager *configManager = nullptr;
    QString modelType;
    ModelConfig currentConfig;
    bool isEditing = false;
    
    // UI components
    QVBoxLayout *mainLayout = nullptr;
    QScrollArea *scrollArea = nullptr;
    QWidget *scrollContent = nullptr;
    QVBoxLayout *contentLayout = nullptr;
    
    // Provider selection
    QGroupBox *providerGroup = nullptr;
    QVBoxLayout *providerLayout = nullptr;
    QComboBox *providerCombo = nullptr;
    QLabel *providerLabel = nullptr;
    
    // Real-time settings
    QCheckBox *realtimeCheckBox = nullptr;
    
    // Parameter inputs
    QGroupBox *parameterGroup = nullptr;
    QFormLayout *parameterLayout = nullptr;
    QMap<QString, QLineEdit*> parameterInputs;
    QMap<QString, QLabel*> parameterLabels;
    
    // Buttons
    QWidget *buttonWidget = nullptr;
    QHBoxLayout *buttonLayout = nullptr;
    QPushButton *cancelButton = nullptr;
    QPushButton *okButton = nullptr;
};

#endif // MODELCONFIGDIALOG_H
