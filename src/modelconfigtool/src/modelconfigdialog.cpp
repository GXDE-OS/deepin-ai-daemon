// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelconfigdialog.h"
#include "logcategory.h"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QTimer>

ModelConfigDialog::ModelConfigDialog(ConfigManager *configManager, const QString &modelType, QWidget *parent)
    : QDialog(parent)
{
    this->configManager = configManager;
    this->modelType = modelType;
    this->isEditing = false;
    currentConfig.modelType = modelType;
    currentConfig.enabled = true;
    setupUI();
    applyStyles();
}

ModelConfigDialog::ModelConfigDialog(ConfigManager *configManager, const QString &modelType, const ModelConfig &config, QWidget *parent)
    : QDialog(parent)
{
    this->configManager = configManager;
    this->modelType = modelType;
    this->currentConfig = config;
    this->isEditing = true;
    setupUI();
    applyStyles();
    loadConfigData();
}

void ModelConfigDialog::setupUI()
{
    setWindowTitle(isEditing ? "编辑模型配置" : "添加模型配置");
    setModal(true);
    
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);
    
    // Create scroll area for content that might overflow
    scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    scrollContent = new QWidget();
    contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);
    
    setupProviderSelection();
    setupParameterInputs();
    
    // Add stretch to content layout
    contentLayout->addStretch();
    
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);
    
    // Buttons
    buttonWidget = new QWidget();
    buttonLayout = new QHBoxLayout(buttonWidget);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    
    buttonLayout->addStretch();
    
    cancelButton = new QPushButton("取消");
    cancelButton->setFixedSize(80, 36);
    
    okButton = new QPushButton("确定");
    okButton->setFixedSize(80, 36);
    okButton->setDefault(true);
    
    buttonLayout->addWidget(okButton);
    buttonLayout->addSpacing(12);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addWidget(buttonWidget);
    
    // Connect signals
    connect(providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModelConfigDialog::onProviderChanged);
    connect(cancelButton, &QPushButton::clicked, this, &ModelConfigDialog::onCancelClicked);
    connect(okButton, &QPushButton::clicked, this, &ModelConfigDialog::onOkClicked);
    
    // Set initial size based on content
    adjustDialogSize();
}

void ModelConfigDialog::setupProviderSelection()
{
    providerGroup = new QGroupBox("模型类型");
    providerLayout = new QVBoxLayout(providerGroup);
    providerLayout->setContentsMargins(16, 12, 16, 12);  // Reduced margins
    providerLayout->setSpacing(8);  // Reduced spacing
    
    // Provider selection row
    QWidget *providerWidget = new QWidget();
    QHBoxLayout *providerRowLayout = new QHBoxLayout(providerWidget);
    providerRowLayout->setContentsMargins(0, 0, 0, 0);
    providerRowLayout->setSpacing(8);
    
    // Radio buttons for provider selection (styled as selection boxes)
    QStringList providers = configManager->getProvidersForType(modelType);
    
    QWidget *radioContainer = new QWidget();
    QVBoxLayout *radioLayout = new QVBoxLayout(radioContainer);
    radioLayout->setContentsMargins(0, 0, 0, 0);
    radioLayout->setSpacing(8);
    
    // Provider dropdown
    providerCombo = new QComboBox();
    for (const QString &provider : providers) {
        QString displayName = getProviderDisplayName(provider);
        providerCombo->addItem(displayName, provider);
    }
    
    providerLayout->addWidget(providerCombo);
    
    // Real-time speech recognition toggle (only for SpeechToText)
    if (modelType == "SpeechToText") {
        QFrame *separator = new QFrame();
        separator->setFrameShape(QFrame::HLine);
        separator->setFrameStyle(QFrame::Sunken);
        separator->setStyleSheet("color: #E0E0E0;");
        providerLayout->addWidget(separator);
        
        QWidget *realtimeWidget = new QWidget();
        QHBoxLayout *realtimeLayout = new QHBoxLayout(realtimeWidget);
        realtimeLayout->setContentsMargins(0, 0, 0, 0);
        
        QLabel *realtimeLabel = new QLabel("语音听写 & 在线语音合成(流式版)");
        realtimeCheckBox = new QCheckBox();
        realtimeCheckBox->setChecked(true);
        
        realtimeLayout->addWidget(realtimeLabel);
        realtimeLayout->addStretch();
        realtimeLayout->addWidget(realtimeCheckBox);
        
        providerLayout->addWidget(realtimeWidget);
    }
    
    contentLayout->addWidget(providerGroup);
}

void ModelConfigDialog::setupParameterInputs()
{
    parameterGroup = new QGroupBox("配置参数");
    parameterLayout = new QFormLayout(parameterGroup);
    parameterLayout->setContentsMargins(16, 12, 16, 12);  // Reduced margins
    parameterLayout->setVerticalSpacing(10);  // Reduced spacing
    parameterLayout->setHorizontalSpacing(12);
    
    populateParameters();
    
    contentLayout->addWidget(parameterGroup);
}

void ModelConfigDialog::populateParameters()
{
    // Clear existing inputs
    for (auto it = parameterInputs.begin(); it != parameterInputs.end(); ++it) {
        it.value()->deleteLater();
    }
    for (auto it = parameterLabels.begin(); it != parameterLabels.end(); ++it) {
        it.value()->deleteLater();
    }
    parameterInputs.clear();
    parameterLabels.clear();
    
    // Clear layout
    while (parameterLayout->count() > 0) {
        QLayoutItem *item = parameterLayout->takeAt(0);
        delete item;
    }
    
    QString currentProvider = providerCombo->currentData().toString();
    if (currentProvider.isEmpty()) {
        return;
    }
    
    QStringList parameters = configManager->getProviderParameters(currentProvider, modelType);
    
    for (const QString &param : parameters) {
        QLabel *label = new QLabel(param + ":");
        QLineEdit *input = new QLineEdit();
        
        // Set input properties based on parameter type
        if (param.contains("Key") || param.contains("Secret")) {
            input->setPlaceholderText("必填");
            input->setStyleSheet("border: 1px solid #FF6B6B;");
        } else {
            input->setPlaceholderText("选填");
        }
        
        // Set password mode for sensitive fields
        if (param.contains("Secret") || param.contains("Key")) {
            input->setEchoMode(QLineEdit::Password);
            
            // Add show/hide button
            QPushButton *toggleButton = new QPushButton("👁");
            toggleButton->setFixedSize(24, 24);
            toggleButton->setStyleSheet(
                "QPushButton {"
                "    border: none;"
                "    background: transparent;"
                "    font-size: 12px;"
                "}"
                "QPushButton:hover {"
                "    background: #F0F0F0;"
                "    border-radius: 12px;"
                "}"
            );
            
            QWidget *inputWidget = new QWidget();
            QHBoxLayout *inputLayout = new QHBoxLayout(inputWidget);
            inputLayout->setContentsMargins(0, 0, 0, 0);
            inputLayout->setSpacing(4);
            inputLayout->addWidget(input);
            inputLayout->addWidget(toggleButton);
            
            connect(toggleButton, &QPushButton::clicked, [input]() {
                if (input->echoMode() == QLineEdit::Password) {
                    input->setEchoMode(QLineEdit::Normal);
                } else {
                    input->setEchoMode(QLineEdit::Password);
                }
            });
            
            parameterLayout->addRow(label, inputWidget);
        } else {
            parameterLayout->addRow(label, input);
        }
        
        parameterInputs[param] = input;
        parameterLabels[param] = label;
    }
}

void ModelConfigDialog::adjustDialogSize()
{
    // Calculate minimum height needed
    int minHeight = 200; // Base height for title, buttons, etc.
    
    // Add height for provider section
    if (providerGroup) {
        minHeight += providerGroup->sizeHint().height() + 20;
    }
    
    // Add height for parameters
    if (parameterGroup) {
        minHeight += parameterGroup->sizeHint().height() + 20;
    }
    
    // Set size constraints
    int dialogWidth = 500;
    int maxHeight = 600; // Maximum dialog height
    int finalHeight = qMin(maxHeight, qMax(450, minHeight)); // At least 450, at most 600
    
    setFixedSize(dialogWidth, finalHeight);
}

void ModelConfigDialog::onProviderChanged()
{
    populateParameters();
    // Adjust dialog size after parameters change
    QTimer::singleShot(0, this, &ModelConfigDialog::adjustDialogSize);
}

void ModelConfigDialog::onOkClicked()
{
    // Validate required fields
    QString currentProvider = providerCombo->currentData().toString();
    
    for (auto it = parameterInputs.begin(); it != parameterInputs.end(); ++it) {
        const QString &param = it.key();
        QLineEdit *input = it.value();
        
        if ((param.contains("Key") || param.contains("Secret")) && input->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "验证错误", QString("参数 '%1' 是必填项").arg(param));
            input->setFocus();
            return;
        }
    }
    
    accept();
}

void ModelConfigDialog::onCancelClicked()
{
    reject();
}

ModelConfig ModelConfigDialog::getModelConfig() const
{
    ModelConfig config;
    config.modelType = modelType;
    config.provider = providerCombo->currentData().toString();
    config.enabled = true;
    
    for (auto it = parameterInputs.begin(); it != parameterInputs.end(); ++it) {
        const QString &param = it.key();
        const QString &value = it.value()->text().trimmed();
        if (!value.isEmpty()) {
            config.parameters[param] = value;
        }
    }
    
    return config;
}

void ModelConfigDialog::loadConfigData()
{
    if (!isEditing) {
        return;
    }
    
    // Set provider
    int providerIndex = providerCombo->findData(currentConfig.provider);
    if (providerIndex >= 0) {
        providerCombo->setCurrentIndex(providerIndex);
    }
    
    // Wait for parameters to be populated, then set values
    QApplication::processEvents();
    
    // Set parameter values
    for (auto it = currentConfig.parameters.begin(); it != currentConfig.parameters.end(); ++it) {
        const QString &param = it.key();
        const QString &value = it.value();
        
        if (parameterInputs.contains(param)) {
            parameterInputs[param]->setText(value);
        }
    }
    
    // Set realtime checkbox if applicable
    if (realtimeCheckBox) {
        realtimeCheckBox->setChecked(currentConfig.enabled);
    }
}

QString ModelConfigDialog::getProviderDisplayName(const QString &provider) const
{
    return configManager->getProviderName(provider);
}

void ModelConfigDialog::applyStyles()
{
    setStyleSheet(
        "QDialog {"
        "    background-color: white;"
        "}"
        "QGroupBox {"
        "    font-size: 14px;"
        "    font-weight: bold;"
        "    color: #333333;"
        "    border: 1px solid #E0E0E0;"
        "    border-radius: 8px;"
        "    margin-top: 12px;"
        "    padding-top: 8px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 12px;"
        "    padding: 0 8px 0 8px;"
        "    background-color: white;"
        "}"
        "QComboBox {"
        "    border: 1px solid #D0D0D0;"
        "    border-radius: 4px;"
        "    padding: 8px 12px;"
        "    background-color: white;"
        "    min-height: 20px;"
        "}"
        "QComboBox:focus {"
        "    border-color: #007BFF;"
        "}"
        "QComboBox::drop-down {"
        "    border: none;"
        "    width: 20px;"
        "}"
        "QComboBox::down-arrow {"
        "    image: none;"
        "    border-left: 4px solid transparent;"
        "    border-right: 4px solid transparent;"
        "    border-top: 4px solid #666666;"
        "    margin-right: 8px;"
        "}"
        "QLineEdit {"
        "    border: 1px solid #D0D0D0;"
        "    border-radius: 4px;"
        "    padding: 8px 12px;"
        "    background-color: white;"
        "    font-size: 12px;"
        "}"
        "QLineEdit:focus {"
        "    border-color: #007BFF;"
        "}"
        "QLabel {"
        "    color: #333333;"
        "    font-size: 12px;"
        "}"
        "QPushButton {"
        "    border: 1px solid #D0D0D0;"
        "    border-radius: 4px;"
        "    padding: 8px 16px;"
        "    background-color: white;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #F8F9FA;"
        "    border-color: #007BFF;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #E9ECEF;"
        "}"
        "QPushButton:default {"
        "    background-color: #007BFF;"
        "    color: white;"
        "    border-color: #007BFF;"
        "}"
        "QPushButton:default:hover {"
        "    background-color: #0056B3;"
        "}"
        "QCheckBox {"
        "    font-size: 12px;"
        "}"
        "QCheckBox::indicator {"
        "    width: 18px;"
        "    height: 18px;"
        "    border-radius: 9px;"
        "    border: 2px solid #D0D0D0;"
        "    background-color: white;"
        "}"
        "QCheckBox::indicator:checked {"
        "    background-color: #007BFF;"
        "    border-color: #007BFF;"
        "}"
        "QCheckBox::indicator:checked:after {"
        "    content: '✓';"
        "    color: white;"
        "    font-size: 12px;"
        "}"
    );
}
