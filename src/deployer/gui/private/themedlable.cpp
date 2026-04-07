#include "themedlable.h"
#include "logcategory.h"

#include <DGuiApplicationHelper>

AIDAEMON_USE_NAMESPACE

ThemedLable::ThemedLable(QWidget *parent, Qt::WindowFlags f) : DLabel(parent, f)
{
    qCDebug(logDeployerUI) << "Creating ThemedLable with default constructor";
    setForegroundRole(QPalette::Text);
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, &ThemedLable::onThemeTypeChanged);
}

ThemedLable::ThemedLable(const QString &text, QWidget *parent) : DLabel(text, parent)
{
    qCDebug(logDeployerUI) << "Creating ThemedLable with text:" << text;
    setForegroundRole(QPalette::Text);
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged, this, &ThemedLable::onThemeTypeChanged);
}

void ThemedLable::setPaletteColor(DPalette::ColorRole role, DPalette::ColorType type, qreal alphaF)
{
    qCDebug(logDeployerUI) << "Setting palette color with ColorType for role:" << role << "alphaF:" << alphaF;
    
    m_colorRoleMap.remove(role);
    m_colorTypeMap.insert(role, std::pair<DPalette::ColorType, qreal>(type, alphaF));

    DPalette pl = this->palette();
    QColor color = DGuiApplicationHelper::instance()->applicationPalette().color(type);
    color.setAlphaF(alphaF);
    pl.setColor(role, color);
    this->setPalette(pl);
}

void ThemedLable::setPaletteColor(DPalette::ColorRole role, DPalette::ColorRole role1, qreal alphaF)
{
    qCDebug(logDeployerUI) << "Setting palette color with ColorRole for role:" << role << "role1:" << role1 << "alphaF:" << alphaF;
    
    m_colorTypeMap.remove(role);
    m_colorRoleMap.insert(role, std::pair<DPalette::ColorRole, qreal>(role1, alphaF));

    DPalette pl = this->palette();
    QColor color = DGuiApplicationHelper::instance()->applicationPalette().color(role1);
    color.setAlphaF(alphaF);
    pl.setColor(role, color);
    this->setPalette(pl);
}

void ThemedLable::onThemeTypeChanged()
{
    qCDebug(logDeployerUI) << "Theme type changed, updating ThemedLable palette";
    
    DPalette pl = this->palette();
    for (auto t : m_colorTypeMap.keys()) {
        QColor color = DGuiApplicationHelper::instance()->applicationPalette().color(m_colorTypeMap.value(t).first);
        color.setAlphaF(m_colorTypeMap.value(t).second);
        pl.setColor(t, color);
    }
    for (auto t : m_colorRoleMap.keys()) {
        QColor color = DGuiApplicationHelper::instance()->applicationPalette().color(m_colorRoleMap.value(t).first);
        color.setAlphaF(m_colorRoleMap.value(t).second);
        pl.setColor(t, color);
    }
    this->setPalette(pl);
    setForegroundRole(QPalette::Text);
}
