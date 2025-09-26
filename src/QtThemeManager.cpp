#include "../include/QtThemeManager.h"
#include <QApplication>
#include <QStyleFactory>

QtThemeManager &QtThemeManager::instance() {
  static QtThemeManager instance;
  return instance;
}

QtThemeManager::QtThemeManager(QObject *parent) : QObject(parent) {
  setupCryptoDarkTheme();
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
  m_primaryColor = QColor(45, 55, 72);
  m_secondaryColor = QColor(68, 80, 102);
  m_backgroundColor = QColor(26, 32, 44);
  m_surfaceColor = QColor(45, 55, 72);
  m_textColor = QColor(248, 250, 252);
  m_accentColor = QColor(66, 153, 225);
  m_errorColor = QColor(245, 101, 101);
  m_successColor = QColor(72, 187, 120);
  m_warningColor = QColor(237, 137, 54);

  m_titleFont = QFont("Segoe UI", 16, QFont::Bold);
  m_buttonFont = QFont("Segoe UI", 10, QFont::Medium);
  m_textFont = QFont("Segoe UI", 10);
  m_monoFont = QFont("Consolas", 10);

  // Dynamic, theme-aware subtitle color (slightly dimmer than main text)
  m_subtitleColor = m_textColor.darker(170);

  // Focus border color - use success color for focus indication
  m_focusBorderColor = m_successColor;
}

void QtThemeManager::setupLightTheme() {
  m_primaryColor = QColor(255, 255, 255);
  m_secondaryColor = QColor(237, 242, 247);
  m_backgroundColor = QColor(248, 250, 252);
  m_surfaceColor = QColor(230, 230, 245);
  m_textColor = QColor(25, 25, 25);
  m_accentColor = QColor(66, 153, 225);
  m_errorColor = QColor(229, 62, 62);
  m_successColor = QColor(56, 161, 105);
  m_warningColor = QColor(221, 107, 32);

  m_titleFont = QFont("Segoe UI", 16, QFont::Bold);
  m_buttonFont = QFont("Segoe UI", 10, QFont::Medium);
  m_textFont = QFont("Segoe UI", 10);
  m_monoFont = QFont("Consolas", 10);

  // Dynamic subtitle on light backgrounds: subtle neutral gray
  m_subtitleColor = QColor(107, 114, 128); // ~ Tailwind gray-500

  // Focus border color - use success color for focus indication
  m_focusBorderColor = m_successColor;
}

void QtThemeManager::setupCryptoDarkTheme() {
  m_primaryColor = QColor(18, 18, 18);
  m_secondaryColor = QColor(30, 30, 30);
  m_backgroundColor = QColor(13, 13, 13);
  m_surfaceColor = QColor(25, 25, 25);
  m_textColor = QColor(255, 255, 255);
  m_accentColor = QColor(80, 20, 60);   // Deep wine/purple burgundy
  m_errorColor = QColor(220, 38, 38);
  m_successColor = QColor(34, 197, 94);
  m_warningColor = QColor(150, 90, 45); // Warmer brown-orange

  m_titleFont = QFont("Segoe UI", 18, QFont::Bold);
  m_buttonFont = QFont("Segoe UI", 11, QFont::Medium);
  m_textFont = QFont("Segoe UI", 10);
  m_monoFont = QFont("JetBrains Mono", 10);

  // Dynamic subtitle on dark backgrounds: slightly dimmer
  m_subtitleColor = m_textColor.darker(170);

  // Focus border color - use success color for focus indication
  m_focusBorderColor = m_successColor;
}

void QtThemeManager::setupCryptoLightTheme() {
  // Light version of CryptoDark theme with improved contrast
  m_primaryColor = QColor(248, 250, 252);     // Very light gray/white
  m_secondaryColor = QColor(148, 163, 184);   // Medium gray for better contrast
  m_backgroundColor = QColor(255, 255, 255);  // Pure white background
  m_surfaceColor = QColor(241, 245, 249);     // Slightly darker surface for contrast
  m_textColor = QColor(15, 23, 42);           // Dark text for contrast
  m_accentColor = QColor(120, 40, 80);        // Wine/purple burgundy (lighter)
  m_errorColor = QColor(239, 68, 68);         // Red for errors
  m_successColor = QColor(34, 197, 94);       // Green for success
  m_warningColor = QColor(245, 158, 11);      // Amber for warnings

  m_titleFont = QFont("Segoe UI", 18, QFont::Bold);
  m_buttonFont = QFont("Segoe UI", 11, QFont::Medium);
  m_textFont = QFont("Segoe UI", 10);
  m_monoFont = QFont("JetBrains Mono", 10);

  // Dynamic subtitle on light background: subtle neutral gray
  m_subtitleColor = QColor(107, 114, 128); // ~ Tailwind gray-500

  // Focus border color - use accent color for focus indication
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
      .arg(textColor().name())         // %1: text color
      .arg(textFont().family())        // %2: base font family
      .arg(textFont().pointSize())     // %3: normal font size
      .arg(titleFont().pointSize())    // %4: title size
      .arg(textFont().pointSize() - 2) // %5: subtitle slightly smaller
      .arg(m_subtitleColor.name())     // %6: subtitle color
      .arg(monoFont().family())        // %7: monospace font for address
      .arg(accentColor().name())       // %8: background for address
      .arg(textFont().pointSize() + 4); // %9: balance font size
}

QString QtThemeManager::getMainWindowStyleSheet() const {
  return QString(R"(
        QMainWindow {
            background-color: %1;
            color: %2;
        }
        QWidget {
            background-color: %3;
            color: %4;
        }
        QFrame[class="card"] {
            background-color: %5;
            border: 1px solid %6;
            border-radius: 12px;
            padding: 16px;
            box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
        }
        QFrame[class="navbar"] {
            background-color: %7;
            border-bottom: 2px solid %8;
            border-radius: 0px;
            box-shadow: 0 2px 6px rgba(0, 0, 0, 0.1);
        }
        QLabel[class="navbar-title"] {
            font-family: %9;
            font-size: 30px;
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
      .arg(m_backgroundColor.name()) // %3 - widget background
      .arg(m_textColor.name())       // %4 - widget text color
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
