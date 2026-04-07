#ifndef THEMEDLABLE_H
#define THEMEDLABLE_H
#include "aidaemon_global.h"
#include <DLabel>
#include <DPalette>

#include <QWidget>

AIDAEMON_BEGIN_NAMESPACE
class ThemedLable : public DTK_WIDGET_NAMESPACE::DLabel
{
public:
    explicit ThemedLable(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ThemedLable(const QString &text, QWidget *parent = nullptr);

    void setPaletteColor(DPalette::ColorRole, DPalette::ColorType, qreal alphaF = 1.0);
    void setPaletteColor(DPalette::ColorRole, DPalette::ColorRole, qreal alphaF = 1.0);

private slots:
    void onThemeTypeChanged();

private:
    QMap<DPalette::ColorRole,  std::pair<DPalette::ColorType, qreal>> m_colorTypeMap;
    QMap<DPalette::ColorRole,  std::pair<DPalette::ColorRole, qreal>> m_colorRoleMap;
};
AIDAEMON_END_NAMESPACE

#endif // THEMEDLABLE_H
