#include "QtThemeManager.h"
#include <QApplication>
#include <QStyleFactory>

QtThemeManager &QtThemeManager::instance() {
  static QtThemeManager instance;
  return instance;
}

QtThemeManager::QtThemeManager(QObject *parent) : QObject(parent) {
  setupCryptoDarkTheme();
}

int QtThemeManager::spacing(int scale) const {
  static const QMap<int, int> spacingScale = {
    {0, 0},    // 0px
    {1, 4},    // 4px
    {2, 8},    // 8px
    {3, 12},   // 12px
    {4, 16},   // 16px
    {5, 20},   // 20px
    {6, 24},   // 24px
    {8, 32},   // 32px
    {10, 40},  // 40px
    {12, 48},  // 48px
    {16, 64},  // 64px
    {20, 80},  // 80px
    {24, 96}   // 96px
  };
  return spacingScale.value(scale, 16); // Default to 16px
}

void QtThemeManager::applyTheme(ThemeType theme) {
  m_currentTheme = theme;

  switch (theme) {
  case ThemeType::DARK:
    setupDarkTheme();
    break;
  case ThemeType::LIGHT:
    setupLightTheme();
    break;
  case ThemeType::CRYPTO_DARK:
    setupCryptoDarkTheme();
    break;
  case ThemeType::CRYPTO_LIGHT:
    setupCryptoLightTheme();
    break;
  }

  updateApplicationStyle();
  emit themeChanged(theme);
}

void QtThemeManager::applyTheme(const QString &themeName) {
  if (themeName == "dark") {
    applyTheme(ThemeType::DARK);
  } else if (themeName == "light") {
    applyTheme(ThemeType::LIGHT);
  } else if (themeName == "crypto-dark") {
    applyTheme(ThemeType::CRYPTO_DARK);
  } else if (themeName == "crypto-light") {
    applyTheme(ThemeType::CRYPTO_LIGHT);
  }
}

void QtThemeManager::setupDarkTheme() {
  m_primaryColor = QColor(51, 65, 85);      // Slate-700
  m_secondaryColor = QColor(100, 116, 139); // Slate-500 - FIXED: better contrast (was 71,85,105)
  m_backgroundColor = QColor(15, 23, 42);   // Slate-900
  m_surfaceColor = QColor(30, 41, 59);      // Slate-800 - distinct from primary
  m_textColor = QColor(248, 250, 252);      // Slate-50
  m_accentColor = QColor(59, 130, 246);     // Blue-500 - more vibrant
  m_errorColor = QColor(239, 68, 68);       // Red-500
  m_successColor = QColor(34, 197, 94);     // Green-500
  m_warningColor = QColor(245, 158, 11);    // Amber-500

  // Semantic colors
  m_positiveColor = QColor(34, 197, 94);    // Green-500 (same as success)
  m_negativeColor = QColor(239, 68, 68);    // Red-500 (same as error)
  m_infoColor = QColor(59, 130, 246);       // Blue-500 (same as accent)

  // Tinted backgrounds (15% opacity)
  m_lightPositiveColor = QColor(34, 197, 94, 38);   // Green with 15% alpha
  m_lightNegativeColor = QColor(239, 68, 68, 38);   // Red with 15% alpha
  m_lightWarningColor = QColor(245, 158, 11, 38);   // Amber with 15% alpha
  m_lightErrorColor = QColor(239, 68, 68, 38);      // Red with 15% alpha
  m_lightInfoColor = QColor(59, 130, 246, 38);      // Blue with 15% alpha

  // Dimmed text variants
  m_dimmedTextColor = QColor(148, 163, 184);        // Slate-400
  m_disabledTextColor = QColor(71, 85, 105);        // Slate-600

  // Border colors
  m_defaultBorderColor = QColor(100, 116, 139);     // Slate-500
  m_errorBorderColor = QColor(239, 68, 68);         // Red-500
  m_successBorderColor = QColor(34, 197, 94);       // Green-500

  m_titleFont = QFont("Segoe UI", 16, QFont::Bold);
  m_buttonFont = QFont("Segoe UI", 10, QFont::Medium);
  m_textFont = QFont("Segoe UI", 10);
  m_monoFont = QFont("Consolas", 10);

  // Dynamic, theme-aware subtitle color (slightly dimmer than main text)
  m_subtitleColor = QColor(148, 163, 184); // Slate-400 - consistent subtle text

  // Focus border color - use accent color for consistency
  m_focusBorderColor = m_accentColor;
}

void QtThemeManager::setupLightTheme() {
  m_primaryColor = QColor(255, 255, 255);   // White
  m_secondaryColor = QColor(148, 163, 184); // Slate-400 - FIXED: better contrast (was 226,232,240)
  m_backgroundColor = QColor(248, 250, 252);// Slate-50
  m_surfaceColor = QColor(241, 245, 249);   // Slate-100 - distinct from white
  m_textColor = QColor(15, 23, 42);         // Slate-900 - better contrast
  m_accentColor = QColor(59, 130, 246);     // Blue-500 - matches dark theme
  m_errorColor = QColor(239, 68, 68);       // Red-500 - matches dark theme
  m_successColor = QColor(34, 197, 94);     // Green-500 - matches dark theme
  m_warningColor = QColor(245, 158, 11);    // Amber-500 - matches dark theme

  // Semantic colors
  m_positiveColor = QColor(34, 197, 94);    // Green-500 (same as success)
  m_negativeColor = QColor(239, 68, 68);    // Red-500 (same as error)
  m_infoColor = QColor(59, 130, 246);       // Blue-500 (same as accent)

  // Tinted backgrounds (15% opacity)
  m_lightPositiveColor = QColor(34, 197, 94, 38);   // Green with 15% alpha
  m_lightNegativeColor = QColor(239, 68, 68, 38);   // Red with 15% alpha
  m_lightWarningColor = QColor(245, 158, 11, 38);   // Amber with 15% alpha
  m_lightErrorColor = QColor(239, 68, 68, 38);      // Red with 15% alpha
  m_lightInfoColor = QColor(59, 130, 246, 38);      // Blue with 15% alpha

  // Dimmed text variants
  m_dimmedTextColor = QColor(100, 116, 139);        // Slate-500
  m_disabledTextColor = QColor(148, 163, 184);      // Slate-400

  // Border colors
  m_defaultBorderColor = QColor(148, 163, 184);     // Slate-400
  m_errorBorderColor = QColor(239, 68, 68);         // Red-500
  m_successBorderColor = QColor(34, 197, 94);       // Green-500

  m_titleFont = QFont("Segoe UI", 16, QFont::Bold);
  m_buttonFont = QFont("Segoe UI", 10, QFont::Medium);
  m_textFont = QFont("Segoe UI", 10);
  m_monoFont = QFont("Consolas", 10);

  // Dynamic subtitle on light backgrounds: subtle neutral gray
  m_subtitleColor = QColor(100, 116, 139); // Slate-500 - consistent with dark

  // Focus border color - use accent color for consistency
  m_focusBorderColor = m_accentColor;
}

void QtThemeManager::setupCryptoDarkTheme() {
  m_primaryColor = QColor(24, 24, 27);      // Zinc-900
  m_secondaryColor = QColor(63, 63, 70);    // Zinc-700 - FIXED: much better contrast (was 39,39,42)
  m_backgroundColor = QColor(9, 9, 11);     // Zinc-950 - true black
  m_surfaceColor = QColor(24, 24, 27);      // Zinc-900 - distinct layer
  m_textColor = QColor(250, 250, 250);      // Zinc-50
  m_accentColor = QColor(168, 85, 247);     // Purple-500 - vibrant accent
  m_errorColor = QColor(239, 68, 68);       // Red-500 - matches other themes
  m_successColor = QColor(34, 197, 94);     // Green-500 - matches other themes
  m_warningColor = QColor(245, 158, 11);    // Amber-500 - matches other themes

  // Semantic colors
  m_positiveColor = QColor(34, 197, 94);    // Green-500 (same as success)
  m_negativeColor = QColor(239, 68, 68);    // Red-500 (same as error)
  m_infoColor = QColor(168, 85, 247);       // Purple-500 (same as accent)

  // Tinted backgrounds (15% opacity)
  m_lightPositiveColor = QColor(34, 197, 94, 38);   // Green with 15% alpha
  m_lightNegativeColor = QColor(239, 68, 68, 38);   // Red with 15% alpha
  m_lightWarningColor = QColor(245, 158, 11, 38);   // Amber with 15% alpha
  m_lightErrorColor = QColor(239, 68, 68, 38);      // Red with 15% alpha
  m_lightInfoColor = QColor(168, 85, 247, 38);      // Purple with 15% alpha

  // Dimmed text variants
  m_dimmedTextColor = QColor(161, 161, 170);        // Zinc-400
  m_disabledTextColor = QColor(63, 63, 70);         // Zinc-700

  // Border colors
  m_defaultBorderColor = QColor(63, 63, 70);        // Zinc-700
  m_errorBorderColor = QColor(239, 68, 68);         // Red-500
  m_successBorderColor = QColor(34, 197, 94);       // Green-500

  m_titleFont = QFont("Segoe UI", 18, QFont::Bold);
  m_buttonFont = QFont("Segoe UI", 11, QFont::Medium);
  m_textFont = QFont("Segoe UI", 10);
  m_monoFont = QFont("JetBrains Mono", 10);

  // Dynamic subtitle on dark backgrounds: slightly dimmer
  m_subtitleColor = QColor(161, 161, 170); // Zinc-400 - consistent subtle text

  // Focus border color - use accent color for consistency
  m_focusBorderColor = m_accentColor;
}

void QtThemeManager::setupCryptoLightTheme() {
  // Light version of CryptoDark theme with improved contrast
  m_primaryColor = QColor(255, 255, 255);   // White
  m_secondaryColor = QColor(161, 161, 170); // Zinc-400 - FIXED: better contrast (was 228,228,231)
  m_backgroundColor = QColor(250, 250, 250);// Zinc-50
  m_surfaceColor = QColor(244, 244, 245);   // Zinc-100 - distinct layer
  m_textColor = QColor(9, 9, 11);           // Zinc-950 - high contrast
  m_accentColor = QColor(168, 85, 247);     // Purple-500 - matches crypto dark
  m_errorColor = QColor(239, 68, 68);       // Red-500 - matches other themes
  m_successColor = QColor(34, 197, 94);     // Green-500 - matches other themes
  m_warningColor = QColor(245, 158, 11);    // Amber-500 - matches other themes

  // Semantic colors
  m_positiveColor = QColor(34, 197, 94);    // Green-500 (same as success)
  m_negativeColor = QColor(239, 68, 68);    // Red-500 (same as error)
  m_infoColor = QColor(168, 85, 247);       // Purple-500 (same as accent)

  // Tinted backgrounds (15% opacity)
  m_lightPositiveColor = QColor(34, 197, 94, 38);   // Green with 15% alpha
  m_lightNegativeColor = QColor(239, 68, 68, 38);   // Red with 15% alpha
  m_lightWarningColor = QColor(245, 158, 11, 38);   // Amber with 15% alpha
  m_lightErrorColor = QColor(239, 68, 68, 38);      // Red with 15% alpha
  m_lightInfoColor = QColor(168, 85, 247, 38);      // Purple with 15% alpha

  // Dimmed text variants
  m_dimmedTextColor = QColor(113, 113, 122);        // Zinc-500
  m_disabledTextColor = QColor(161, 161, 170);      // Zinc-400

  // Border colors
  m_defaultBorderColor = QColor(161, 161, 170);     // Zinc-400
  m_errorBorderColor = QColor(239, 68, 68);         // Red-500
  m_successBorderColor = QColor(34, 197, 94);       // Green-500

  m_titleFont = QFont("Segoe UI", 18, QFont::Bold);
  m_buttonFont = QFont("Segoe UI", 11, QFont::Medium);
  m_textFont = QFont("Segoe UI", 10);
  m_monoFont = QFont("JetBrains Mono", 10);

  // Dynamic subtitle on light background: subtle neutral gray
  m_subtitleColor = QColor(113, 113, 122); // Zinc-500 - consistent with crypto dark

  // Focus border color - use accent color for consistency
  m_focusBorderColor = m_accentColor;
}

void QtThemeManager::updateApplicationStyle() {
  if (QApplication *app =
          qobject_cast<QApplication *>(QCoreApplication::instance())) {
    app->setStyleSheet(getMainWindowStyleSheet());
  }
}

QString QtThemeManager::getButtonStyleSheet() const {
  return QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: 2px solid %3;
            border-radius: 8px;
            padding: 8px 16px;
            font-family: %4;
            font-size: %5px;
            font-weight: 600;
            min-height: 20px;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
        }
        QPushButton:hover {
            background-color: %6;
            border-color: %7;
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.15);
        }
        QPushButton:pressed {
            background-color: %8;
            transform: translateY(1px);
        }
        QPushButton:disabled {
            background-color: %9;
            color: %10;
            border-color: %11;
        }
    )")
      .arg(m_surfaceColor.name())
      .arg(m_textColor.name())
      .arg(m_accentColor.name())
      .arg(m_buttonFont.family())
      .arg(m_buttonFont.pointSize())
      .arg(m_accentColor.lighter(120).name())
      .arg(m_accentColor.lighter(130).name())
      .arg(m_accentColor.darker(120).name())
      .arg(m_surfaceColor.darker(150).name())
      .arg(m_textColor.darker(200).name())
      .arg(m_accentColor.darker(200).name());
}

QString QtThemeManager::getLineEditStyleSheet() const {
  return QString(R"(
        QLineEdit {
            background-color: %1;
            color: %2;
            border: 2px solid %3;
            border-radius: 6px;
            padding: 8px 12px;
            font-family: %4;
            font-size: %5px;
            selection-background-color: %6;
            box-shadow: inset 0 1px 3px rgba(0, 0, 0, 0.1);
        }
        QLineEdit:focus {
            border-color: %7;
            background-color: %1;
            box-shadow: inset 0 1px 3px rgba(0, 0, 0, 0.1);
        }
        QLineEdit:disabled {
            background-color: %9;
            color: %10;
            border-color: %11;
        }
    )")
      .arg(m_surfaceColor.name())
      .arg(m_textColor.name())
      .arg(m_secondaryColor.name()) // border uses secondary; Crypto Light sets
                                    // this to a darker gray
      .arg(m_textFont.family())
      .arg(m_textFont.pointSize())
      .arg(m_accentColor.lighter(150).name())
      .arg(m_accentColor.name())
      .arg(m_surfaceColor.lighter(105).name())
      .arg(m_surfaceColor.darker(120).name())
      .arg(m_textColor.darker(150).name())
      .arg(m_secondaryColor.darker(150).name())
      .arg(m_focusBorderColor.name());
}

QString QtThemeManager::getLabelStyleSheet() const {
  // Bright title (accent), subtle/dynamic subtitle (m_subtitleColor)
  return QString(R"(
        QLabel {
            color: %1;
            font-family: %2;
            font-size: %3px;
            background-color: transparent;
            border: none;
        }
        QLabel[class="title"] {
            font-family: %2;
            font-size: %4px;
            font-weight: 700;
            color: %1;
        }
        QLabel[class="subtitle"] {
            font-family: %2;
            font-size: %5px;
            color: %6;
        }
        QLabel[class="balance-title"] {
            font-family: %2;
            font-size: 14px;
            font-weight: 500;
            color: %1;
        }
        QLabel[class="wallet-balance"] {
            font-family: %2;
            font-size: %9px;
            color: %1;
        }
        QLabel[class="address"] {
            font-family: %7;
            font-size: %3px;
            color: %1;
            background-color: %8;
            padding: 4px 8px;
            border-radius: 4px;
        }
    )")
      .arg(textColor().name())          // %1: text color
      .arg(textFont().family())         // %2: base font family
      .arg(textFont().pointSize())      // %3: normal font size
      .arg(titleFont().pointSize())     // %4: title size
      .arg(textFont().pointSize() - 2)  // %5: subtitle slightly smaller
      .arg(m_subtitleColor.name())      // %6: subtitle color
      .arg(monoFont().family())         // %7: monospace font for address
      .arg(accentColor().name())        // %8: background for address
      .arg(textFont().pointSize() + 4); // %9: balance font size
}

QString QtThemeManager::getMainWindowStyleSheet() const {
  return QString(R"(
        QMainWindow {
            background-color: %1;
            color: %2;
        }
        QMainWindow > QWidget#contentContainer {
            background-color: %3;
        }
        QFrame[class="card"] {
            background-color: %5;
            border: 1px solid %6;
            border-radius: 12px;
            padding: 10px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
        }
        QFrame[class="navbar"] {
            background-color: %7;
            border-bottom: 2px solid %8;
            border-radius: 8px;
            box-shadow: 0 2px 6px rgba(0, 0, 0, 0.1);
        }
        QLabel[class="navbar-title"] {
            font-family: %9;
            font-size: 35px;
            font-weight: bold;
            color: %10;
        }
        QPushButton[class="navbar-button"] {
            background-color: %11;
            color: %12;
            border: 1px solid %13;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: 500;
        }
        QPushButton[class="navbar-button"]:hover {
            background-color: %14;
            border-color: %15;
        }
    )")
      .arg(m_backgroundColor.name()) // %1 - main background
      .arg(m_textColor.name())       // %2 - main text color
      .arg(m_backgroundColor.name()) // %3 - content container background
      .arg(m_textColor.name())       // %4 - widget text color (unused now)
      .arg(m_surfaceColor.name())    // %5 - card background
      .arg(m_secondaryColor.name())  // %6 - card border
      .arg(m_backgroundColor.name()) // %7 - navbar background
      .arg(m_accentColor.name())     // %8 - navbar border
      .arg(m_titleFont.family())     // %9 - navbar title font
      .arg(m_accentColor.name())     // %10 - navbar title color
      .arg(m_surfaceColor.name())    // %11 - navbar button background
      .arg(m_textColor.name())       // %12 - navbar button text
      .arg(m_secondaryColor.name())  // %13 - navbar button border
      .arg(m_surfaceColor.lighter(180)
               .name())           // %14 - navbar button hover background
      .arg(m_accentColor.name()); // %15 - navbar button hover border
}

QString QtThemeManager::getCardStyleSheet() const {
  return QString(R"(
        QFrame {
            background-color: %1;
            border: 2px solid %2;
            border-radius: 12px;
            padding: 20px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
        }
        QFrame:hover {
            border-color: %2;
            box-shadow: 0 6px 16px rgba(0, 0, 0, 0.15);
        }
    )")
      .arg(m_surfaceColor.name())
      .arg(m_accentColor.name());
}

QString QtThemeManager::getMessageStyleSheet() const {
  return QString(R"(
        QLabel {
            padding: 8px;
            border-radius: 6px;
            font-weight: 500;
            border: none;
            color: %1;
        }
    )")
      .arg(m_textColor.name());
}

QString QtThemeManager::getErrorMessageStyleSheet() const {
  return getMessageStyleSheet() + QString(R"(
        QLabel {
            color: #ffffff;
            background-color: %1;
        }
    )")
                                      .arg(m_errorColor.name());
}

QString QtThemeManager::getSuccessMessageStyleSheet() const {
  return getMessageStyleSheet() + QString(R"(
        QLabel {
            color: #ffffff;
            background-color: %1;
        }
    )")
                                      .arg(m_successColor.name());
}
