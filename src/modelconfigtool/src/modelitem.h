// SPDX-FileCopyrightText: 2025 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELITEM_H
#define MODELITEM_H

#include "configmanager.h"

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMenu>

class ModelItem : public QWidget
{
    Q_OBJECT

public:
    explicit ModelItem(const ModelConfig &config, int index, const QString &displayName = QString(), QWidget *parent = nullptr);
    
    const ModelConfig& getConfig() const { return config; }
    int getIndex() const { return index; }

signals:
    void editRequested(int index);
    void deleteRequested(int index);
    void setAsDefaultRequested(int index);

private slots:
    void onMenuButtonClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupUI();
    void applyStyles();
    
    ModelConfig config;
    int index = 0;
    QString displayName;
    
    QHBoxLayout *layout = nullptr;
    QLabel *iconLabel = nullptr;
    QLabel *nameLabel = nullptr;
    QPushButton *menuButton = nullptr;
    QMenu *menu = nullptr;
};

#endif // MODELITEM_H
