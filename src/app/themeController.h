#pragma once
// themeController.h
// Resolves the effective color scheme (dark/light) from the user's theme
// preference and the desktop's color scheme, following live changes via
// QStyleHints::colorSchemeChanged. QML binds Theme.qml colors to `dark`.

#include <QObject>
#include <QQmlEngine>

class ThemeController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum class Mode { System, Dark, Light }; // mirrors AppConfig::Theme
    Q_ENUM(Mode)

    Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)

    static ThemeController* instance();
    static ThemeController* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    bool dark() const { return m_dark; }
    Mode mode() const { return m_mode; }
    void setMode(Mode mode);

signals:
    void darkChanged();
    void modeChanged();

private:
    explicit ThemeController(QObject* parent = nullptr);
    void resolveDark();
    bool systemPrefersDark() const;

    Mode m_mode = Mode::System;
    bool m_dark = true;
};
