#include "../include/QtThemeManager.h"
#include <QApplication>
#include <QStyleFactory>

QtThemeManager& QtThemeManager::instance() {
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

void QtThemeManager::applyTheme(const QString& themeName) {
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
}

void QtThemeManager::setupLightTheme() {
    m_primaryColor = QColor(255, 255, 255);
    m_secondaryColor = QColor(237, 242, 247);
    m_backgroundColor = QColor(248, 250, 252);
    m_surfaceColor = QColor(255, 255, 255);
    m_textColor = QColor(45, 55, 72);
    m_accentColor = QColor(66, 153, 225);
    m_errorColor = QColor(229, 62, 62);
    m_successColor = QColor(56, 161, 105);
    m_warningColor = QColor(221, 107, 32);
    
    m_titleFont = QFont("Segoe UI", 16, QFont::Bold);
    m_buttonFont = QFont("Segoe UI", 10, QFont::Medium);
    m_textFont = QFont("Segoe UI", 10);
    m_monoFont = QFont("Consolas", 10);
}

void QtThemeManager::setupCryptoDarkTheme() {
    m_primaryColor = QColor(18, 18, 18);
    m_secondaryColor = QColor(30, 30, 30);
    m_backgroundColor = QColor(13, 13, 13);
    m_surfaceColor = QColor(25, 25, 25);
    m_textColor = QColor(255, 255, 255);
    m_accentColor = QColor(255, 165, 0);
    m_errorColor = QColor(220, 38, 38);
    m_successColor = QColor(34, 197, 94);
    m_warningColor = QColor(251, 146, 60);
    
    m_titleFont = QFont("Segoe UI", 18, QFont::Bold);
    m_buttonFont = QFont("Segoe UI", 11, QFont::Medium);
    m_textFont = QFont("Segoe UI", 10);
    m_monoFont = QFont("JetBrains Mono", 10);
}

void QtThemeManager::setupCryptoLightTheme() {
    m_primaryColor = QColor(255, 255, 255);
    m_secondaryColor = QColor(249, 250, 251);
    m_backgroundColor = QColor(255, 255, 255);
    m_surfaceColor = QColor(248, 250, 252);
    m_textColor = QColor(17, 24, 39);
    m_accentColor = QColor(245, 158, 11);
    m_errorColor = QColor(239, 68, 68);
    m_successColor = QColor(34, 197, 94);
    m_warningColor = QColor(245, 158, 11);
    
    m_titleFont = QFont("Segoe UI", 18, QFont::Bold);
    m_buttonFont = QFont("Segoe UI", 11, QFont::Medium);
    m_textFont = QFont("Segoe UI", 10);
    m_monoFont = QFont("JetBrains Mono", 10);
}

void QtThemeManager::updateApplicationStyle() {
    if (QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance())) {
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
        }
        QPushButton:hover {
            background-color: %6;
            border-color: %7;
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
    )").arg(m_surfaceColor.name())
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
        }
        QLineEdit:focus {
            border-color: %7;
            background-color: %8;
        }
        QLineEdit:disabled {
            background-color: %9;
            color: %10;
            border-color: %11;
        }
    )").arg(m_surfaceColor.name())
       .arg(m_textColor.name())
       .arg(m_secondaryColor.name())
       .arg(m_textFont.family())
       .arg(m_textFont.pointSize())
       .arg(m_accentColor.lighter(150).name())
       .arg(m_accentColor.name())
       .arg(m_surfaceColor.lighter(105).name())
       .arg(m_surfaceColor.darker(120).name())
       .arg(m_textColor.darker(150).name())
       .arg(m_secondaryColor.darker(150).name());
}

QString QtThemeManager::getLabelStyleSheet() const {
    return QString(R"(
        QLabel {
            color: %1;
            font-family: %2;
            font-size: %3px;
            background-color: transparent;
        }
        QLabel[class="title"] {
            font-family: %4;
            font-size: %5px;
            font-weight: 700;
            color: %6;
        }
        QLabel[class="subtitle"] {
            font-size: %7px;
            color: %8;
        }
        QLabel[class="address"] {
            font-family: %9;
            font-size: %10px;
            color: %11;
            background-color: %12;
            padding: 4px 8px;
            border-radius: 4px;
        }
    )").arg(m_textColor.name())
       .arg(m_textFont.family())
       .arg(m_textFont.pointSize())
       .arg(m_titleFont.family())
       .arg(m_titleFont.pointSize())
       .arg(m_accentColor.name())
       .arg(m_textFont.pointSize() - 1)
       .arg(m_textColor.darker(130).name())
       .arg(m_monoFont.family())
       .arg(m_monoFont.pointSize())
       .arg(m_accentColor.name())
       .arg(m_surfaceColor.name());
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
        }
    )").arg(m_backgroundColor.name())
       .arg(m_textColor.name())
       .arg(m_backgroundColor.name())
       .arg(m_textColor.name())
       .arg(m_surfaceColor.name())
       .arg(m_secondaryColor.name());
}

QString QtThemeManager::getCardStyleSheet() const {
    return QString(R"(
        QFrame {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 12px;
            padding: 20px;
        }
        QFrame:hover {
            border-color: %3;
        }
    )").arg(m_surfaceColor.name())
       .arg(m_secondaryColor.name())
       .arg(m_accentColor.name());
}