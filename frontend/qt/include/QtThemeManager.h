#pragma once

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>

enum class ThemeType { DARK, LIGHT, CRYPTO_DARK, CRYPTO_LIGHT };

class QtThemeManager : public QObject {
    Q_OBJECT

  public:
    static QtThemeManager& instance();

    void applyTheme(ThemeType theme);
    void applyTheme(const QString& themeName);
    ThemeType getCurrentTheme() const {
        return m_currentTheme;
    }

    // Color getters
    QColor primaryColor() const {
        return m_primaryColor;
    }
    QColor secondaryColor() const {
        return m_secondaryColor;
    }
    QColor backgroundColor() const {
        return m_backgroundColor;
    }
    QColor surfaceColor() const {
        return m_surfaceColor;
    }
    QColor textColor() const {
        return m_textColor;
    }
    QColor accentColor() const {
        return m_accentColor;
    }
    QColor errorColor() const {
        return m_errorColor;
    }
    QColor successColor() const {
        return m_successColor;
    }
    QColor warningColor() const {
        return m_warningColor;
    }
    QColor subtitleColor() const {
        return m_subtitleColor;
    }
    QColor focusBorderColor() const {
        return m_focusBorderColor;
    }

    // Semantic colors
    QColor positiveColor() const {
        return m_positiveColor;
    }
    QColor negativeColor() const {
        return m_negativeColor;
    }
    QColor infoColor() const {
        return m_infoColor;
    }

    // Tinted backgrounds (with alpha)
    QColor lightPositive() const {
        return m_lightPositiveColor;
    }
    QColor lightNegative() const {
        return m_lightNegativeColor;
    }
    QColor lightWarning() const {
        return m_lightWarningColor;
    }
    QColor lightError() const {
        return m_lightErrorColor;
    }
    QColor lightInfo() const {
        return m_lightInfoColor;
    }

    // Dimmed text variants
    QColor dimmedTextColor() const {
        return m_dimmedTextColor;
    }
    QColor disabledTextColor() const {
        return m_disabledTextColor;
    }

    // Border colors
    QColor defaultBorderColor() const {
        return m_defaultBorderColor;
    }
    QColor errorBorderColor() const {
        return m_errorBorderColor;
    }
    QColor successBorderColor() const {
        return m_successBorderColor;
    }

    // Spacing system (based on 4px grid)
    int spacing(int scale) const;
    int compactSpacing() const {
        return 12;
    }
    int standardSpacing() const {
        return 16;
    }
    int generousSpacing() const {
        return 24;
    }

    int compactMargin() const {
        return 16;
    }
    int standardMargin() const {
        return 24;
    }
    int generousMargin() const {
        return 32;
    }

    // Border radius
    int borderRadiusSmall() const {
        return 4;
    }
    int borderRadiusMedium() const {
        return 8;
    }
    int borderRadiusLarge() const {
        return 12;
    }
    int borderRadiusXLarge() const {
        return 16;
    }
    int borderRadiusFull() const {
        return 9999;
    }

    // Font getters
    QFont titleFont() const {
        return m_titleFont;
    }
    QFont buttonFont() const {
        return m_buttonFont;
    }
    QFont textFont() const {
        return m_textFont;
    }
    QFont monoFont() const {
        return m_monoFont;
    }

    // Style sheet generators
    QString getButtonStyleSheet() const;
    QString getOutlinedButtonStyleSheet() const;
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
    QtThemeManager(QObject* parent = nullptr);
    ~QtThemeManager() override = default;
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
    QColor m_focusBorderColor;

    // Semantic colors
    QColor m_positiveColor;
    QColor m_negativeColor;
    QColor m_infoColor;

    // Tinted backgrounds
    QColor m_lightPositiveColor;
    QColor m_lightNegativeColor;
    QColor m_lightWarningColor;
    QColor m_lightErrorColor;
    QColor m_lightInfoColor;

    // Dimmed text
    QColor m_dimmedTextColor;
    QColor m_disabledTextColor;

    // Border colors
    QColor m_defaultBorderColor;
    QColor m_errorBorderColor;
    QColor m_successBorderColor;

    // Typography
    QFont m_titleFont;
    QFont m_buttonFont;
    QFont m_textFont;
    QFont m_monoFont;
};
