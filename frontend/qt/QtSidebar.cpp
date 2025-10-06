#include "QtSidebar.h"
#include "QtThemeManager.h"

#include <QLabel>
#include <QSpacerItem>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>

QtSidebar::QtSidebar(QtThemeManager *themeManager, QWidget *parent)
    : QWidget(parent), m_themeManager(themeManager), m_isExpanded(false) {

    setupUI();
    applyTheme();

    // Connect to theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtSidebar::applyTheme);
}

void QtSidebar::setupUI() {
    // Set initial size for overlay positioning
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
    m_menuButton = new QPushButton(m_sidebarContent);
    m_menuButton->setProperty("class", "sidebar-menu-button");
    m_menuButton->setCursor(Qt::PointingHandCursor);
    m_menuButton->setToolTip("Toggle Menu");
    m_menuButton->setFixedSize(50, 50);
    m_navLayout->addWidget(m_menuButton);

    // Connect menu button to toggle sidebar
    connect(m_menuButton, &QPushButton::clicked, this, &QtSidebar::toggleSidebar);

    // Add some spacing after menu button
    m_navLayout->addSpacing(20);

    createNavigationButtons();

    // Add stretch to push buttons to top
    m_navLayout->addStretch();

    m_mainLayout->addWidget(m_sidebarContent);

    // Setup animation for width (using geometry)
    m_widthAnimation = new QPropertyAnimation(this, "geometry");
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
    m_walletButton->setFixedHeight(50);

    QHBoxLayout *walletLayout = new QHBoxLayout(m_walletButton);
    walletLayout->setContentsMargins(8, 0, 8, 0);
    walletLayout->setSpacing(12);

    walletLayout->addStretch();
    QLabel *walletIcon = new QLabel(m_walletButton);
    walletIcon->setObjectName("walletIcon");
    walletIcon->setAlignment(Qt::AlignCenter);
    walletIcon->setFixedSize(24, 24);
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
    m_settingsButton->setFixedHeight(50);

    QHBoxLayout *settingsLayout = new QHBoxLayout(m_settingsButton);
    settingsLayout->setContentsMargins(8, 0, 8, 0);
    settingsLayout->setSpacing(12);

    settingsLayout->addStretch();
    QLabel *settingsIcon = new QLabel(m_settingsButton);
    settingsIcon->setObjectName("settingsIcon");
    settingsIcon->setAlignment(Qt::AlignCenter);
    settingsIcon->setFixedSize(24, 24);
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
    int currentHeight = height();

    // Update fixed width immediately
    setFixedWidth(targetWidth);

    // Animate geometry change while maintaining height
    QRect startGeometry = geometry();
    QRect endGeometry(0, 0, targetWidth, currentHeight);

    m_widthAnimation->setStartValue(startGeometry);
    m_widthAnimation->setEndValue(endGeometry);

    // Update button text visibility based on state
    QLabel *walletText = m_walletButton->findChild<QLabel *>("walletText");
    QLabel *settingsText = m_settingsButton->findChild<QLabel *>("settingsText");

    if (walletText) {
        walletText->setVisible(expand);
    }
    if (settingsText) {
        settingsText->setVisible(expand);
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

QIcon QtSidebar::createColoredIcon(const QString &svgPath, const QColor &color) {
    QSvgRenderer renderer(svgPath);
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter);
    painter.end();

    // Colorize the icon
    QPixmap coloredPixmap(24, 24);
    coloredPixmap.fill(Qt::transparent);

    QPainter colorPainter(&coloredPixmap);
    colorPainter.setCompositionMode(QPainter::CompositionMode_Source);
    colorPainter.drawPixmap(0, 0, pixmap);
    colorPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    colorPainter.fillRect(coloredPixmap.rect(), color);
    colorPainter.end();

    return QIcon(coloredPixmap);
}

void QtSidebar::applyTheme() {
    // Determine if theme is dark or light based on background luminance
    bool isDarkTheme = m_themeManager->surfaceColor().lightness() < 128;

    // Use grey tones that match the theme direction but remain clearly grey
    // For dark themes: use a LIGHTER neutral grey (stays grey, doesn't blend with dark background)
    // For light themes: use a DARKER neutral grey (stays grey, doesn't blend with light background)
    QString sidebarBg = isDarkTheme ? "#505050" : "#383838";  // Pure greys
    QString borderColor = isDarkTheme ? "#606060" : "#2a2a2a";
    QString hoverColor = isDarkTheme ? "#5a5a5a" : "#484848";
    QString pressedColor = isDarkTheme ? "#656565" : "#555555";
    QString textColor = isDarkTheme ? "#e8e8e8" : "#d0d0d0";

    QString sidebarStyle = QString(R"(
        QtSidebar {
            background-color: %1;
            border-right: 3px solid %2;
        }
        QPushButton[class="sidebar-menu-button"] {
            background-color: transparent;
            border: none;
            border-radius: 12px;
            padding: 0px;
            text-align: center;
        }
        QPushButton[class="sidebar-menu-button"]:hover {
            background-color: %3;
        }
        QPushButton[class="sidebar-menu-button"]:pressed {
            background-color: %4;
        }
        QPushButton[class="sidebar-nav-button"] {
            background-color: transparent;
            color: %5;
            border: none;
            border-radius: 12px;
            padding: 0px;
            text-align: left;
        }
        QPushButton[class="sidebar-nav-button"]:hover {
            background-color: %3;
        }
        QPushButton[class="sidebar-nav-button"]:pressed {
            background-color: %4;
        }
        QLabel {
            background-color: transparent;
            color: %5;
        }
    )")
        .arg(sidebarBg)
        .arg(borderColor)
        .arg(hoverColor)
        .arg(pressedColor)
        .arg(textColor);

    setStyleSheet(sidebarStyle);

    // Create colored SVG icons that match text color
    QColor iconColor = isDarkTheme ? QColor(232, 232, 232) : QColor(208, 208, 208);
    QIcon menuIcon = createColoredIcon(":/icons/icons/menu.svg", iconColor);
    QIcon walletIcon = createColoredIcon(":/icons/icons/wallet.svg", iconColor);
    QIcon settingsIcon = createColoredIcon(":/icons/icons/settings.svg", iconColor);

    // Set icons on buttons
    m_menuButton->setIcon(menuIcon);
    m_menuButton->setIconSize(QSize(24, 24));

    // Set icons on navigation buttons
    QLabel *walletIconLabel = m_walletButton->findChild<QLabel *>("walletIcon");
    QLabel *settingsIconLabel = m_settingsButton->findChild<QLabel *>("settingsIcon");

    if (walletIconLabel) {
        QPixmap walletPixmap = walletIcon.pixmap(QSize(24, 24));
        walletIconLabel->setPixmap(walletPixmap);
    }

    if (settingsIconLabel) {
        QPixmap settingsPixmap = settingsIcon.pixmap(QSize(24, 24));
        settingsIconLabel->setPixmap(settingsPixmap);
    }

    // Apply fonts to button labels
    QFont textFont = m_themeManager->buttonFont();
    textFont.setPointSize(13);
    textFont.setWeight(QFont::Medium);

    // Apply to wallet button text
    QLabel *walletText = m_walletButton->findChild<QLabel *>("walletText");
    if (walletText) walletText->setFont(textFont);

    // Apply to settings button text
    QLabel *settingsText = m_settingsButton->findChild<QLabel *>("settingsText");
    if (settingsText) settingsText->setFont(textFont);
}
