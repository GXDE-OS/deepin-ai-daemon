// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "modelitem.h"
#include <QPixmap>
#include <QApplication>
#include <QMouseEvent>

ModelItem::ModelItem(const ModelConfig &config, int index, const QString &displayName, QWidget *parent)
    : QWidget(parent)
{
    this->config = config;
    this->index = index;
    this->displayName = displayName.isEmpty() ? config.provider : displayName;
    setupUI();
    applyStyles();
}

void ModelItem::setupUI()
{
    layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(10);
    
    // Status icon (blue check mark when enabled)
    iconLabel = new QLabel();
    iconLabel->setFixedSize(20, 20);
    if (config.enabled) {
        iconLabel->setStyleSheet(
            "QLabel {"
            "    background-color: #007BFF;"
            "    border-radius: 10px;"
            "    color: white;"
            "}"
        );
        iconLabel->setText("✓");
        iconLabel->setAlignment(Qt::AlignCenter);
    } else {
        iconLabel->setStyleSheet(
            "QLabel {"
            "    background-color: #E0E0E0;"
            "    border-radius: 10px;"
            "}"
        );
    }
    
    // Model name label
    nameLabel = new QLabel(displayName);
    nameLabel->setFont(QFont("Noto Sans CJK SC", 11));
    
    // Menu button (three dots)
    menuButton = new QPushButton("⋯");
    menuButton->setFixedSize(32, 32);
    menuButton->setStyleSheet(
        "QPushButton {"
        "    border: none;"
        "    background-color: transparent;"
        "    font-size: 16px;"
        "    color: #666666;"
        "    border-radius: 16px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #F0F0F0;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #E0E0E0;"
        "}"
    );
    
    // Create context menu
    menu = new QMenu(this);
    menu->addAction("编辑", [this]() { emit editRequested(index); });
    menu->addAction("删除", [this]() { emit deleteRequested(index); });
    
    // Layout
    layout->addWidget(iconLabel);
    layout->addWidget(nameLabel, 1);
    layout->addWidget(menuButton);
    
    // Connect signals
    connect(menuButton, &QPushButton::clicked, this, &ModelItem::onMenuButtonClicked);
}

void ModelItem::applyStyles()
{
    setStyleSheet(
        "ModelItem {"
        "    background-color: white;"
        "    border: 1px solid #E0E0E0;"
        "    border-radius: 8px;"
        "    margin: 4px 0px;"
        "}"
        "ModelItem:hover {"
        "    background-color: #F0F8FF;"
        "    border-color: #007BFF;"
        "}"
    );
    
    // Set cursor to indicate clickable
    setCursor(Qt::PointingHandCursor);
    
    // Set fixed height
    setFixedHeight(56);
}

void ModelItem::onMenuButtonClicked()
{
    QPoint globalPos = menuButton->mapToGlobal(QPoint(0, menuButton->height()));
    menu->exec(globalPos);
}

void ModelItem::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if click is not on the menu button
        QRect menuButtonRect = menuButton->geometry();
        if (!menuButtonRect.contains(event->pos())) {
            // Emit signal to set this model as default
            emit setAsDefaultRequested(index);
        }
    }
    QWidget::mousePressEvent(event);
}
