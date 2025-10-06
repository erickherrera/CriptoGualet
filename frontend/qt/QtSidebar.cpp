#include "QtSidebar.h"
#include "QtThemeManager.h"

#include <QLabel>
#include <QSpacerItem>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>

QtSidebar::QtSidebar(QtThemeManager *themeManager, QWidget *parent)
    : QWidget(parent), m_themeManager(themeManager), m_isExpanded(false) {

    setupUI();
    applyTheme();

    // Connect to theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtSidebar::applyTheme);
}

void QtSidebar::setupUI() {
    // Set fixed height to full parent height
    setFixedWidth(COLLAPSED_WIDTH);

    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Sidebar content widget
    m_sidebarContent = new QWidget(this);
    m_navLayout = new QVBoxLayout(m_sidebarContent);
    m_navLayout->setContentsMargins(10, 20, 10, 20);
    m_navLayout->setSpacing(15);

    // Hamburger menu button at the top
    QPushButton *menuButton = new QPushButton("☰", m_sidebarContent);
    menuButton->setProperty("class", "sidebar-menu-button");
    menuButton->setCursor(Qt::PointingHandCursor);
    menuButton->setToolTip("Toggle Menu");
    m_navLayout->addWidget(menuButton);

    // Connect menu button to toggle sidebar
    connect(menuButton, &QPushButton::clicked, this, &QtSidebar::toggleSidebar);

    // Add some spacing after menu button
    m_navLayout->addSpacing(20);

    createNavigationButtons();

    // Add stretch to push buttons to top
    m_navLayout->addStretch();

    m_mainLayout->addWidget(m_sidebarContent);

    // Setup animations
    m_widthAnimation = new QPropertyAnimation(this, "maximumWidth");
    m_widthAnimation->setDuration(300);
    m_widthAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    // Install event filter on parent to detect clicks outside
    if (parent()) {
        parent()->installEventFilter(this);
    }
}

void QtSidebar::createNavigationButtons() {
    // Wallet button with icon and text
    m_walletButton = new QPushButton(m_sidebarContent);
    m_walletButton->setProperty("class", "sidebar-nav-button");
    m_walletButton->setCursor(Qt::PointingHandCursor);
    m_walletButton->setToolTip("Wallet");

    QHBoxLayout *walletLayout = new QHBoxLayout(m_walletButton);
    walletLayout->setContentsMargins(0, 0, 0, 0);
    walletLayout->setSpacing(12);

    walletLayout->addStretch();
    QLabel *walletIcon = new QLabel("💼", m_walletButton);
    walletIcon->setObjectName("walletIcon");
    walletIcon->setAlignment(Qt::AlignCenter);
    walletLayout->addWidget(walletIcon);

    QLabel *walletText = new QLabel("Wallet", m_walletButton);
    walletText->setObjectName("walletText");
    walletText->setVisible(false);
    walletLayout->addWidget(walletText);
    walletLayout->addStretch();

    m_navLayout->addWidget(m_walletButton);

    connect(m_walletButton, &QPushButton::clicked, this, [this]() {
        emit navigateToWallet();
        if (m_isExpanded) {
            toggleSidebar();
        }
    });

    // Settings button with icon and text
    m_settingsButton = new QPushButton(m_sidebarContent);
    m_settingsButton->setProperty("class", "sidebar-nav-button");
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    m_settingsButton->setToolTip("Settings");

    QHBoxLayout *settingsLayout = new QHBoxLayout(m_settingsButton);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(12);

    settingsLayout->addStretch();
    QLabel *settingsIcon = new QLabel("⚙️", m_settingsButton);
    settingsIcon->setObjectName("settingsIcon");
    settingsIcon->setAlignment(Qt::AlignCenter);
    settingsLayout->addWidget(settingsIcon);

    QLabel *settingsText = new QLabel("Settings", m_settingsButton);
    settingsText->setObjectName("settingsText");
    settingsText->setVisible(false);
    settingsLayout->addWidget(settingsText);
    settingsLayout->addStretch();

    m_navLayout->addWidget(m_settingsButton);

    connect(m_settingsButton, &QPushButton::clicked, this, [this]() {
        emit navigateToSettings();
        if (m_isExpanded) {
            toggleSidebar();
        }
    });
}

void QtSidebar::toggleSidebar() {
    m_isExpanded = !m_isExpanded;
    animateSidebar(m_isExpanded);
}

void QtSidebar::animateSidebar(bool expand) {
    int targetWidth = expand ? EXPANDED_WIDTH : COLLAPSED_WIDTH;

    m_widthAnimation->setStartValue(width());
    m_widthAnimation->setEndValue(targetWidth);

    setFixedWidth(targetWidth);

    // Update button text visibility and layout based on state
    QLabel *walletText = m_walletButton->findChild<QLabel *>("walletText");
    QLabel *settingsText = m_settingsButton->findChild<QLabel *>("settingsText");

    if (walletText) {
        walletText->setVisible(expand);
    }
    if (settingsText) {
        settingsText->setVisible(expand);
    }

    // Adjust button alignment based on state
    if (expand) {
        // Left align when expanded
        m_walletButton->setStyleSheet("");
        m_settingsButton->setStyleSheet("");
    }

    m_widthAnimation->start();
}

bool QtSidebar::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress && m_isExpanded) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

        // Check if click was outside sidebar
        if (!geometry().contains(mouseEvent->pos())) {
            toggleSidebar();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void QtSidebar::applyTheme() {
    QString sidebarStyle = QString(R"(
        QtSidebar {
            background-color: %1;
            border-right: 2px solid %2;
        }
        QPushButton[class="sidebar-menu-button"] {
            background-color: transparent;
            color: %3;
            border: none;
            border-radius: 10px;
            padding: 10px;
            text-align: center;
            font-size: 24px;
            font-weight: 400;
            min-height: 40px;
            margin: 2px 6px;
        }
        QPushButton[class="sidebar-menu-button"]:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %4, stop:1 %6);
        }
        QPushButton[class="sidebar-menu-button"]:pressed {
            background-color: %5;
        }
        QPushButton[class="sidebar-nav-button"] {
            background-color: transparent;
            color: %3;
            border: none;
            border-radius: 10px;
            padding: 10px;
            text-align: center;
            font-size: 14px;
            font-weight: 500;
            min-height: 48px;
            margin: 2px 6px;
        }
        QPushButton[class="sidebar-nav-button"]:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %4, stop:1 %6);
        }
        QPushButton[class="sidebar-nav-button"]:pressed {
            background-color: %5;
        }
        QLabel {
            background-color: transparent;
            color: %3;
        }
    )")
        .arg(m_themeManager->surfaceColor().name())
        .arg(m_themeManager->surfaceColor().darker(110).name())
        .arg(m_themeManager->textColor().name())
        .arg(m_themeManager->accentColor().lighter(170).name())
        .arg(m_themeManager->accentColor().lighter(150).name())
        .arg(m_themeManager->accentColor().lighter(180).name());

    setStyleSheet(sidebarStyle);

    // Apply fonts to button labels
    QFont iconFont = m_themeManager->buttonFont();
    iconFont.setPointSize(16);

    QFont textFont = m_themeManager->buttonFont();
    textFont.setPointSize(13);
    textFont.setWeight(QFont::Medium);

    // Apply to wallet button
    QLabel *walletIcon = m_walletButton->findChild<QLabel *>("walletIcon");
    QLabel *walletText = m_walletButton->findChild<QLabel *>("walletText");
    if (walletIcon) walletIcon->setFont(iconFont);
    if (walletText) walletText->setFont(textFont);

    // Apply to settings button
    QLabel *settingsIcon = m_settingsButton->findChild<QLabel *>("settingsIcon");
    QLabel *settingsText = m_settingsButton->findChild<QLabel *>("settingsText");
    if (settingsIcon) settingsIcon->setFont(iconFont);
    if (settingsText) settingsText->setFont(textFont);
}
