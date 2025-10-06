#include "QtSettingsUI.h"
#include "QtThemeManager.h"

#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>

QtSettingsUI::QtSettingsUI(QWidget *parent)
    : QWidget(parent), m_themeManager(&QtThemeManager::instance()) {

    setupUI();
    applyTheme();

    // Connect to theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtSettingsUI::applyTheme);
}

void QtSettingsUI::setupUI() {
    // Main layout with horizontal centering
    QHBoxLayout *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Center container with max width
    QWidget *centerContainer = new QWidget(this);
    centerContainer->setMaximumWidth(900);

    m_mainLayout = new QVBoxLayout(centerContainer);
    m_mainLayout->setContentsMargins(40, 40, 40, 40);
    m_mainLayout->setSpacing(30);

    // Add stretch before and after to center the container
    outerLayout->addStretch();
    outerLayout->addWidget(centerContainer);
    outerLayout->addStretch();

    // Title
    m_titleLabel = new QLabel("Settings", centerContainer);
    m_titleLabel->setProperty("class", "settings-title");
    m_titleLabel->setAlignment(Qt::AlignLeft);
    m_mainLayout->addWidget(m_titleLabel);

    // Theme Settings Group
    QGroupBox *themeGroup = new QGroupBox("Appearance", centerContainer);
    QFormLayout *themeLayout = new QFormLayout(themeGroup);
    themeLayout->setContentsMargins(20, 20, 20, 20);
    themeLayout->setSpacing(15);

    // Theme selector
    m_themeSelector = new QComboBox(centerContainer);
    m_themeSelector->addItem("Dark Theme", static_cast<int>(ThemeType::DARK));
    m_themeSelector->addItem("Light Theme", static_cast<int>(ThemeType::LIGHT));
    m_themeSelector->addItem("Crypto Dark", static_cast<int>(ThemeType::CRYPTO_DARK));
    m_themeSelector->addItem("Crypto Light", static_cast<int>(ThemeType::CRYPTO_LIGHT));

    // Set current theme
    ThemeType currentTheme = m_themeManager->getCurrentTheme();
    m_themeSelector->setCurrentIndex(m_themeSelector->findData(static_cast<int>(currentTheme)));

    connect(m_themeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        int themeValue = m_themeSelector->itemData(index).toInt();
        m_themeManager->applyTheme(static_cast<ThemeType>(themeValue));
    });

    themeLayout->addRow("Theme:", m_themeSelector);

    m_mainLayout->addWidget(themeGroup);

    // Add placeholder sections for future settings
    QGroupBox *securityGroup = new QGroupBox("Security", centerContainer);
    QVBoxLayout *securityLayout = new QVBoxLayout(securityGroup);
    securityLayout->setContentsMargins(20, 20, 20, 20);
    QLabel *securityPlaceholder = new QLabel("Security settings will be added here", centerContainer);
    securityPlaceholder->setStyleSheet("color: gray; font-style: italic;");
    securityLayout->addWidget(securityPlaceholder);
    m_mainLayout->addWidget(securityGroup);

    QGroupBox *walletGroup = new QGroupBox("Wallet", centerContainer);
    QVBoxLayout *walletLayout = new QVBoxLayout(walletGroup);
    walletLayout->setContentsMargins(20, 20, 20, 20);
    QLabel *walletPlaceholder = new QLabel("Wallet settings will be added here", centerContainer);
    walletPlaceholder->setStyleSheet("color: gray; font-style: italic;");
    walletLayout->addWidget(walletPlaceholder);
    m_mainLayout->addWidget(walletGroup);

    // Add stretch at the end
    m_mainLayout->addStretch();
}

void QtSettingsUI::applyTheme() {
    // Main background
    setStyleSheet(QString("QWidget { background-color: %1; }")
                      .arg(m_themeManager->backgroundColor().name()));

    // Title styling
    QString titleStyle = QString(R"(
        QLabel {
            color: %1;
            font-size: 32px;
            font-weight: 700;
            background-color: transparent;
        }
    )").arg(m_themeManager->textColor().name());
    m_titleLabel->setStyleSheet(titleStyle);

    // Group box styling
    QString groupBoxStyle = QString(R"(
        QGroupBox {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            font-size: 16px;
            font-weight: 600;
            color: %3;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 15px;
            padding: 0 5px;
        }
    )")
        .arg(m_themeManager->surfaceColor().name())
        .arg(m_themeManager->surfaceColor().darker(120).name())
        .arg(m_themeManager->textColor().name());

    // Apply to all group boxes
    for (QGroupBox *groupBox : findChildren<QGroupBox *>()) {
        groupBox->setStyleSheet(groupBoxStyle);
    }

    // Combo box styling
    QString comboBoxStyle = QString(R"(
        QComboBox {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 4px;
            padding: 8px;
            min-width: 200px;
        }
        QComboBox:hover {
            border-color: %4;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid %2;
            margin-right: 8px;
        }
    )")
        .arg(m_themeManager->surfaceColor().name())
        .arg(m_themeManager->textColor().name())
        .arg(m_themeManager->surfaceColor().darker(120).name())
        .arg(m_themeManager->accentColor().name());

    m_themeSelector->setStyleSheet(comboBoxStyle);

    // Apply fonts
    QFont titleFont = m_themeManager->titleFont();
    titleFont.setPointSize(28);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);

    QFont textFont = m_themeManager->textFont();
    textFont.setPointSize(12);
    m_themeSelector->setFont(textFont);
}
