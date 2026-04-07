// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mainwindow.h"
#include "logcategory.h"
#include <QMenuBar>
#include <QStatusBar>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFile>
#include <QDir>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    configManager = new ConfigManager(this);
    setupUI();
    setupMenuBar();
    applyStyles();
    
    // Load default config file using standard paths
    QStringList configPaths;
    
    // Try standard configuration paths in order of preference
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString systemConfigPath = configDir + "/deepin/deepin-ai-daemon/modelproviders.json";
    
    // First try the standard user config location
    configPaths << systemConfigPath;
    
    // Then try application directory (for development/testing)
    configPaths << QCoreApplication::applicationDirPath() + "/modelproviders.json";
    
    // Finally try current directory (fallback)
    configPaths << "modelproviders.json";
    
    QString loadedConfigPath;
    for (const QString &path : configPaths) {
        qCDebug(logModelConfigTool) << "Trying config path:" << path;
        if (!path.isEmpty() && QFile::exists(path)) {
            qCInfo(logModelConfigTool) << "Found config file at:" << path;
            if (configManager->loadConfig(path)) {
                loadedConfigPath = path;
                currentConfigFile = path;
                qCInfo(logModelConfigTool) << "Successfully loaded config from:" << path;
                break;
            } else {
                qCWarning(logModelConfigTool) << "Failed to load config from:" << path;
            }
        }
    }
    
    if (!loadedConfigPath.isEmpty()) {
        statusBar()->showMessage("已加载配置文件: " + loadedConfigPath, 3000);
        // Refresh all sections
        for (const QString &modelType : configManager->getModelTypes()) {
            refreshModelTypeSection(modelType);
        }
    } else {
        qCInfo(logModelConfigTool) << "No existing config file found, will create new one";
        currentConfigFile = systemConfigPath;
        
        // Ensure the directory exists
        QDir configDirObj(configDir + "/deepin/deepin-ai-daemon");
        if (!configDirObj.exists()) {
            if (configDirObj.mkpath(".")) {
                qCInfo(logModelConfigTool) << "Created config directory:" << configDirObj.absolutePath();
            } else {
                qCWarning(logModelConfigTool) << "Failed to create config directory:" << configDirObj.absolutePath();
            }
        }
        
        configManager->loadConfig(currentConfigFile);
        statusBar()->showMessage("将创建新的配置文件: " + currentConfigFile);
    }
}

MainWindow::~MainWindow()
{
    delete configManager;
}

void MainWindow::setupUI()
{
    centralWidget = new QWidget();
    setCentralWidget(centralWidget);
    
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(6);
    
    // Create scroll area
    scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    scrollContent = new QWidget();
    scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(8);
    
    // Create model type sections
    createModelTypeSection("Chat", "文本类模型", "处理和生成文本，比如聊天对话、回答问题");
    createModelTypeSection("ImageRecognition", "图像类模型", "处理和生成图像，比如识别物体、分析场景");
    createModelTypeSection("SpeechToText", "语音转文本", "将语音转换为文字，比如语音识别、语音输入");
    createModelTypeSection("TextToSpeech", "文本转语音", "将文字转换为语音，比如语音播报、语音合成");
    createModelTypeSection("FunctionCalling", "功能调用模型", "执行特定功能和任务调用");
    
    // Add stretch to push content to top
    scrollLayout->addStretch();
    
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);
    
    // Set window properties
    setWindowTitle("AI 模型配置");
    setMinimumSize(600, 700);
    resize(700, 800);
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();
    
    // File menu
    QMenu *fileMenu = menuBar->addMenu("文件");
    fileMenu->addAction("打开配置文件", this, &MainWindow::openConfigFile);
    fileMenu->addSeparator();
    fileMenu->addAction("保存", this, &MainWindow::saveConfigFile);
    fileMenu->addSeparator();
    fileMenu->addAction("退出", this, &QWidget::close);
    
    // Set status bar
    statusBar()->showMessage("就绪");
}

void MainWindow::createModelTypeSection(const QString &modelType, const QString &displayName, const QString &description)
{
    QGroupBox *groupBox = new QGroupBox();
    groupBox->setTitle(displayName);
    groupBox->setStyleSheet(
        "QGroupBox {"
        "    font-size: 13px;"
        "    font-weight: bold;"
        "    color: #333333;"
        "    border: 1px solid #E0E0E0;"
        "    border-radius: 6px;"
        "    margin-top: 8px;"
        "    padding-top: 6px;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 8px;"
        "    padding: 0 6px 0 6px;"
        "    background-color: white;"
        "}"
    );
    
    QVBoxLayout *groupLayout = new QVBoxLayout(groupBox);
    groupLayout->setContentsMargins(10, 12, 10, 10);
    groupLayout->setSpacing(6);
    
    // Header with description and add button in same row
    QWidget *headerWidget = new QWidget();
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    
    // Description label
    QLabel *descLabel = new QLabel(description);
    descLabel->setStyleSheet(
        "QLabel {"
        "    color: #666666;"
        "    font-size: 11px;"
        "}"
    );
    descLabel->setWordWrap(true);
    
    // Add button - smaller and more compact
    QPushButton *addButton = new QPushButton("+ 添加");
    addButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #F8F9FA;"
        "    border: 1px dashed #C0C0C0;"
        "    border-radius: 6px;"
        "    padding: 4px 8px;"
        "    color: #666666;"
        "    font-size: 11px;"
        "    min-width: 50px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #E9ECEF;"
        "    border-color: #007BFF;"
        "    color: #007BFF;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #DEE2E6;"
        "}"
    );
    addButton->setFixedHeight(24);
    addButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    connect(addButton, &QPushButton::clicked, [this, modelType]() {
        addModel(modelType);
    });
    
    headerLayout->addWidget(descLabel, 1);
    headerLayout->addWidget(addButton, 0, Qt::AlignTop);
    
    // Models container
    QWidget *modelsContainer = new QWidget();
    QVBoxLayout *modelsLayout = new QVBoxLayout(modelsContainer);
    modelsLayout->setContentsMargins(0, 0, 0, 0);
    modelsLayout->setSpacing(3);
    
    groupLayout->addWidget(headerWidget);
    groupLayout->addWidget(modelsContainer);
    
    scrollLayout->addWidget(groupBox);
    
    // Store references
    modelTypeSections[modelType] = groupBox;
    modelTypeLayouts[modelType] = modelsLayout;
    addButtons[modelType] = addButton;
}

void MainWindow::refreshModelTypeSection(const QString &modelType)
{
    if (!modelTypeLayouts.contains(modelType)) {
        return;
    }
    
    QVBoxLayout *layout = modelTypeLayouts[modelType];
    
    // Clear existing items
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    
    // Add model items
    QList<ModelConfig> models = configManager->getModelsForType(modelType);
    for (int i = 0; i < models.size(); ++i) {
        QString displayName = configManager->getProviderName(models[i].provider);
        ModelItem *item = new ModelItem(models[i], i, displayName);
        connect(item, &ModelItem::editRequested, [this, modelType](int index) {
            editModel(modelType, index);
        });
        connect(item, &ModelItem::deleteRequested, [this, modelType](int index) {
            deleteModel(modelType, index);
        });
        connect(item, &ModelItem::setAsDefaultRequested, [this, modelType](int index) {
            setAsDefaultModel(modelType, index);
        });
        layout->addWidget(item);
    }
}

void MainWindow::openConfigFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "选择配置文件",
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        "配置文件 (*.conf);;所有文件 (*)"
    );
    
    if (!filePath.isEmpty()) {
        if (configManager->loadConfig(filePath)) {
            currentConfigFile = filePath;
            statusBar()->showMessage("已加载配置文件: " + filePath, 3000);
            
            // Refresh all sections
            for (const QString &modelType : configManager->getModelTypes()) {
                refreshModelTypeSection(modelType);
            }
        } else {
            QMessageBox::warning(this, "错误", "无法加载配置文件");
        }
    }
}

void MainWindow::saveConfigFile()
{
    if (currentConfigFile.isEmpty()) {
        QString filePath = QFileDialog::getSaveFileName(
            this,
            "保存配置文件",
            QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/modelproviders.json",
            "配置文件 (*.conf);;所有文件 (*)"
        );
        
        if (!filePath.isEmpty()) {
            currentConfigFile = filePath;
        } else {
            return;
        }
    }
    
    if (configManager->saveConfig(currentConfigFile)) {
        statusBar()->showMessage("配置文件已保存", 3000);
    } else {
        QMessageBox::warning(this, "错误", "无法保存配置文件");
    }
}

void MainWindow::addModel(const QString &modelType)
{
    ModelConfigDialog dialog(configManager, modelType, this);
    if (dialog.exec() == QDialog::Accepted) {
        ModelConfig config = dialog.getModelConfig();
        configManager->addModel(config);
        
        // Save configuration to file
        if (configManager->saveConfig(currentConfigFile)) {
            qCInfo(logModelConfigTool) << "Successfully saved config after adding model";
            statusBar()->showMessage("模型添加成功并已保存", 3000);
        } else {
            qCWarning(logModelConfigTool) << "Failed to save config after adding model";
            statusBar()->showMessage("模型添加成功但保存失败", 5000);
        }
        
        refreshModelTypeSection(modelType);
    }
}

void MainWindow::editModel(const QString &modelType, int index)
{
    QList<ModelConfig> models = configManager->getModelsForType(modelType);
    if (index >= 0 && index < models.size()) {
        ModelConfigDialog dialog(configManager, modelType, models[index], this);
        if (dialog.exec() == QDialog::Accepted) {
            ModelConfig config = dialog.getModelConfig();
            configManager->updateModel(index, config);
            
            // Save configuration to file
            if (configManager->saveConfig(currentConfigFile)) {
                qCInfo(logModelConfigTool) << "Successfully saved config after updating model";
                statusBar()->showMessage("模型更新成功并已保存", 3000);
            } else {
                qCWarning(logModelConfigTool) << "Failed to save config after updating model";
                statusBar()->showMessage("模型更新成功但保存失败", 5000);
            }
            
            refreshModelTypeSection(modelType);
        }
    }
}

void MainWindow::deleteModel(const QString &modelType, int index)
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "确认删除",
        "确定要删除这个模型配置吗？",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        configManager->removeModel(modelType, index);
        
        // Save configuration to file
        if (configManager->saveConfig(currentConfigFile)) {
            qCInfo(logModelConfigTool) << "Successfully saved config after removing model";
            statusBar()->showMessage("模型删除成功并已保存", 3000);
        } else {
            qCWarning(logModelConfigTool) << "Failed to save config after removing model";
            statusBar()->showMessage("模型删除成功但保存失败", 5000);
        }
        
        refreshModelTypeSection(modelType);
    }
}

void MainWindow::setAsDefaultModel(const QString &modelType, int index)
{
    QList<ModelConfig> models = configManager->getModelsForType(modelType);
    if (index >= 0 && index < models.size()) {
        const ModelConfig &selectedModel = models[index];
        
        // Set the selected model's provider as default for this model type
        configManager->setDefaultProvider(modelType, selectedModel.provider);
        
        // Update the enabled status of all models of this type
        configManager->updateModelEnabledStatus(modelType);
        
        // Save configuration to file
        if (configManager->saveConfig(currentConfigFile)) {
            qCInfo(logModelConfigTool) << "Successfully saved config after setting default model";
            QString providerDisplayName = configManager->getProviderName(selectedModel.provider);
            statusBar()->showMessage(QString("已将 %1 设为 %2 的默认模型").arg(providerDisplayName, getModelTypeDisplayName(modelType)), 3000);
        } else {
            qCWarning(logModelConfigTool) << "Failed to save config after setting default model";
            statusBar()->showMessage("设置默认模型成功但保存失败", 5000);
        }
        
        // Refresh the section to update the UI (enabled status)
        refreshModelTypeSection(modelType);
    }
}

void MainWindow::applyStyles()
{
    setStyleSheet(
        "QMainWindow {"
        "    background-color: #F8F9FA;"
        "}"
        "QScrollArea {"
        "    background-color: #F8F9FA;"
        "}"
    );
}

QString MainWindow::getModelTypeDisplayName(const QString &modelType) const
{
    if (modelType == "Chat") {
        return "文本类模型";
    } else if (modelType == "ImageRecognition") {
        return "图像类模型";
    } else if (modelType == "SpeechToText") {
        return "语音转文本";
    } else if (modelType == "TextToSpeech") {
        return "文本转语音";
    } else if (modelType == "FunctionCalling") {
        return "功能调用模型";
    }
    return modelType;
}

QString MainWindow::getModelTypeDescription(const QString &modelType) const
{
    if (modelType == "Chat") {
        return "处理和生成文本，比如聊天对话、回答问题";
    } else if (modelType == "ImageRecognition") {
        return "处理和生成图像，比如识别物体、分析场景";
    } else if (modelType == "SpeechToText") {
        return "将语音转换为文字，比如语音识别、语音输入";
    } else if (modelType == "TextToSpeech") {
        return "将文字转换为语音，比如语音播报、语音合成";
    } else if (modelType == "FunctionCalling") {
        return "执行特定功能和任务调用";
    }
    return "";
}

QString MainWindow::getModelTypeIcon(const QString &modelType) const
{
    if (modelType == "Chat") {
        return "💬";
    } else if (modelType == "ImageRecognition") {
        return "🖼️";
    } else if (modelType == "SpeechToText") {
        return "🎤";
    } else if (modelType == "TextToSpeech") {
        return "🔊";
    } else if (modelType == "FunctionCalling") {
        return "⚙️";
    }
    return "📋";
}
