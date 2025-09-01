#pragma once

#include <QObject>
#include <QString>
#include <QColor>
#include <QFont>
#include <QApplication>

enum class ThemeType {
    DARK,
    LIGHT,
    CRYPTO_DARK,
    CRYPTO_LIGHT
};

class QtThemeManager : public QObject {
    Q_OBJECT

public:
    static QtThemeManager& instance();
    
    void applyTheme(ThemeType theme);
    void applyTheme(const QString& themeName);
    ThemeType getCurrentTheme() const { return m_currentTheme; }
    
    // Color getters
    QColor primaryColor() const { return m_primaryColor; }
    QColor secondaryColor() const { return m_secondaryColor; }
    QColor backgroundColor() const { return m_backgroundColor; }
    QColor surfaceColor() const { return m_surfaceColor; }
    QColor textColor() const { return m_textColor; }
    QColor accentColor() const { return m_accentColor; }
    QColor errorColor() const { return m_errorColor; }
    QColor successColor() const { return m_successColor; }
    QColor warningColor() const { return m_warningColor; }
    QColor subtitleColor() const { return m_subtitleColor; }
    
    // Font getters
    QFont titleFont() const { return m_titleFont; }
    QFont buttonFont() const { return m_buttonFont; }
    QFont textFont() const { return m_textFont; }
    QFont monoFont() const { return m_monoFont; }
    
    // Style sheet generators
    QString getButtonStyleSheet() const;
    QString getLineEditStyleSheet() const;
    QString getLabelStyleSheet() const;
    QString getMainWindowStyleSheet() const;
    QString getCardStyleSheet() const;
    QString getMessageStyleSheet() const;
    QString getErrorMessageStyleSheet() const;
    QString getSuccessMessageStyleSheet() const;

signals:
    void themeChanged(ThemeType newTheme);

private:
    QtThemeManager(QObject *parent = nullptr);
    ~QtThemeManager() = default;
    QtThemeManager(const QtThemeManager&) = delete;
    QtThemeManager& operator=(const QtThemeManager&) = delete;
    
    void setupDarkTheme();
    void setupLightTheme();
    void setupCryptoDarkTheme();
    void setupCryptoLightTheme();
    void updateApplicationStyle();
    
    ThemeType m_currentTheme = ThemeType::CRYPTO_DARK;
    
    // Color scheme
    QColor m_primaryColor;
    QColor m_secondaryColor;
    QColor m_backgroundColor;
    QColor m_surfaceColor;
    QColor m_textColor;
    QColor m_accentColor;
    QColor m_errorColor;
    QColor m_successColor;
    QColor m_warningColor;
    QColor m_subtitleColor;

    
    // Typography
    QFont m_titleFont;
    QFont m_buttonFont;
    QFont m_textFont;
    QFont m_monoFont;
};