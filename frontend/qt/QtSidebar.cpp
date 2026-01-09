#include "QtSidebar.h"
#include "QtThemeManager.h"

#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QSpacerItem>
#include <QSvgRenderer>

QtSidebar::QtSidebar(QtThemeManager* themeManager, QWidget* parent)
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
    m_menuButton->setFixedHeight(50);  // Only fix height, allow width to expand
    m_navLayout->addWidget(m_menuButton);

    // Connect menu button to toggle sidebar
    connect(m_menuButton, &QPushButton::clicked, this, &QtSidebar::toggleSidebar);

    // Add some spacing after menu button
    m_navLayout->addSpacing(20);

    createNavigationButtons();

    // Add stretch to push buttons to top
    m_navLayout->addStretch();

    // Add Sign Out button at the bottom
    m_signOutButton = new QPushButton(m_sidebarContent);
    m_signOutButton->setProperty("class", "sidebar-nav-button signout-button");
    m_signOutButton->setCursor(Qt::PointingHandCursor);
    m_signOutButton->setToolTip("Sign Out");
    m_signOutButton->setFixedHeight(50);

    QHBoxLayout* signOutLayout = new QHBoxLayout(m_signOutButton);
    signOutLayout->setContentsMargins(8, 0, 8, 0);
    signOutLayout->setSpacing(12);

    signOutLayout->addStretch();
    QLabel* signOutIcon = new QLabel(m_signOutButton);
    signOutIcon->setObjectName("signOutIcon");
    signOutIcon->setAlignment(Qt::AlignCenter);
    signOutIcon->setFixedSize(24, 24);
    signOutLayout->addWidget(signOutIcon);

    QLabel* signOutText = new QLabel("Sign Out", m_signOutButton);
    signOutText->setObjectName("signOutText");
    signOutText->setVisible(false);
    signOutLayout->addWidget(signOutText);
    signOutLayout->addStretch();

    m_navLayout->addWidget(m_signOutButton);

    connect(m_signOutButton, &QPushButton::clicked, this, [this]() { emit signOutRequested(); });

    m_mainLayout->addWidget(m_sidebarContent);

    // Setup animation for width (using geometry)
    m_widthAnimation = new QPropertyAnimation(this, "geometry");
    m_widthAnimation->setDuration(300);
    m_widthAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    // Install event filter on parent to detect clicks outside
    if (parent()) {
        parent()->installEventFilter(this);
    }

    // Install event filter on buttons for hover labels
    m_walletButton->installEventFilter(this);
    m_topCryptosButton->installEventFilter(this);
    m_settingsButton->installEventFilter(this);
    m_signOutButton->installEventFilter(this);
}

void QtSidebar::createNavigationButtons() {
    // Wallet button with icon and text
    m_walletButton = new QPushButton(m_sidebarContent);
    m_walletButton->setProperty("class", "sidebar-nav-button");
    m_walletButton->setCursor(Qt::PointingHandCursor);
    m_walletButton->setToolTip("Wallet");
    m_walletButton->setFixedHeight(50);

    QHBoxLayout* walletLayout = new QHBoxLayout(m_walletButton);
    walletLayout->setContentsMargins(8, 0, 8, 0);
    walletLayout->setSpacing(12);

    walletLayout->addStretch();
    QLabel* walletIcon = new QLabel(m_walletButton);
    walletIcon->setObjectName("walletIcon");
    walletIcon->setAlignment(Qt::AlignCenter);
    walletIcon->setFixedSize(24, 24);
    walletLayout->addWidget(walletIcon);

    QLabel* walletText = new QLabel("Wallet", m_walletButton);
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

    // Top Cryptos button with icon and text
    m_topCryptosButton = new QPushButton(m_sidebarContent);
    m_topCryptosButton->setProperty("class", "sidebar-nav-button");
    m_topCryptosButton->setCursor(Qt::PointingHandCursor);
    m_topCryptosButton->setToolTip("Top Cryptos");
    m_topCryptosButton->setFixedHeight(50);

    QHBoxLayout* topCryptosLayout = new QHBoxLayout(m_topCryptosButton);
    topCryptosLayout->setContentsMargins(8, 0, 8, 0);
    topCryptosLayout->setSpacing(12);

    topCryptosLayout->addStretch();
    QLabel* topCryptosIcon = new QLabel(m_topCryptosButton);
    topCryptosIcon->setObjectName("topCryptosIcon");
    topCryptosIcon->setAlignment(Qt::AlignCenter);
    topCryptosIcon->setFixedSize(24, 24);
    topCryptosLayout->addWidget(topCryptosIcon);

    QLabel* topCryptosText = new QLabel("Markets", m_topCryptosButton);
    topCryptosText->setObjectName("topCryptosText");
    topCryptosText->setVisible(false);
    topCryptosLayout->addWidget(topCryptosText);
    topCryptosLayout->addStretch();

    m_navLayout->addWidget(m_topCryptosButton);

    connect(m_topCryptosButton, &QPushButton::clicked, this, [this]() {
        emit navigateToTopCryptos();
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

    QHBoxLayout* settingsLayout = new QHBoxLayout(m_settingsButton);
    settingsLayout->setContentsMargins(8, 0, 8, 0);
    settingsLayout->setSpacing(12);

    settingsLayout->addStretch();
    QLabel* settingsIcon = new QLabel(m_settingsButton);
    settingsIcon->setObjectName("settingsIcon");
    settingsIcon->setAlignment(Qt::AlignCenter);
    settingsIcon->setFixedSize(24, 24);
    settingsLayout->addWidget(settingsIcon);

    QLabel* settingsText = new QLabel("Settings", m_settingsButton);
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

    // Update fixed width immediately
    setFixedWidth(targetWidth);

    // Emit signal so parent layout can adjust content margin
    emit sidebarWidthChanged(targetWidth);

    // Update button text visibility based on state
    QLabel* walletText = m_walletButton->findChild<QLabel*>("walletText");
    QLabel* topCryptosText = m_topCryptosButton->findChild<QLabel*>("topCryptosText");
    QLabel* settingsText = m_settingsButton->findChild<QLabel*>("settingsText");
    QLabel* signOutText = m_signOutButton->findChild<QLabel*>("signOutText");

    if (walletText) {
        walletText->setVisible(expand);
    }
    if (topCryptosText) {
        topCryptosText->setVisible(expand);
    }
    if (settingsText) {
        settingsText->setVisible(expand);
    }
    if (signOutText) {
        signOutText->setVisible(expand);
    }
}

bool QtSidebar::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress && m_isExpanded) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

        // Check if click was outside sidebar
        if (!geometry().contains(mouseEvent->pos())) {
            toggleSidebar();
            return true;
        }
    }

    // Handle hover labels when collapsed
    if (!m_isExpanded) {
        if (obj == m_walletButton || obj == m_topCryptosButton || obj == m_settingsButton ||
            obj == m_signOutButton) {
            if (event->type() == QEvent::Enter) {
                QLabel* textLabel =
                    obj->findChild<QLabel*>(obj->objectName().replace("Button", "Text"));
                if (!textLabel) {
                    // Fallback to manual check if objectName not set
                    if (obj == m_walletButton)
                        textLabel = m_walletButton->findChild<QLabel*>("walletText");
                    else if (obj == m_topCryptosButton)
                        textLabel = m_topCryptosButton->findChild<QLabel*>("topCryptosText");
                    else if (obj == m_settingsButton)
                        textLabel = m_settingsButton->findChild<QLabel*>("settingsText");
                    else if (obj == m_signOutButton)
                        textLabel = m_signOutButton->findChild<QLabel*>("signOutText");
                }
                if (textLabel)
                    textLabel->setVisible(true);
            } else if (event->type() == QEvent::Leave) {
                QLabel* textLabel =
                    obj->findChild<QLabel*>(obj->objectName().replace("Button", "Text"));
                if (!textLabel) {
                    if (obj == m_walletButton)
                        textLabel = m_walletButton->findChild<QLabel*>("walletText");
                    else if (obj == m_topCryptosButton)
                        textLabel = m_topCryptosButton->findChild<QLabel*>("topCryptosText");
                    else if (obj == m_settingsButton)
                        textLabel = m_settingsButton->findChild<QLabel*>("settingsText");
                    else if (obj == m_signOutButton)
                        textLabel = m_signOutButton->findChild<QLabel*>("signOutText");
                }
                if (textLabel)
                    textLabel->setVisible(false);
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

QIcon QtSidebar::createColoredIcon(const QString& svgPath, const QColor& color) {
    // Render at higher resolution for better quality (2x scaling)
    const int size = 48;
    const int displaySize = 24;

    QSvgRenderer renderer(svgPath);
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    // Enable high-quality rendering
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter);
    painter.end();

    // Colorize the icon
    QPixmap coloredPixmap(size, size);
    coloredPixmap.fill(Qt::transparent);

    QPainter colorPainter(&coloredPixmap);
    colorPainter.setRenderHint(QPainter::Antialiasing, true);
    colorPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    colorPainter.setCompositionMode(QPainter::CompositionMode_Source);
    colorPainter.drawPixmap(0, 0, pixmap);
    colorPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    colorPainter.fillRect(coloredPixmap.rect(), color);
    colorPainter.end();

    // Scale down to display size for crisp rendering
    QPixmap scaledPixmap = coloredPixmap.scaled(displaySize, displaySize, Qt::KeepAspectRatio,
                                                Qt::SmoothTransformation);

    return QIcon(scaledPixmap);
}

void QtSidebar::applyTheme() {
    // Define gray tones for each theme
    QString sidebarBg, borderColor, textColor;
    QColor iconColor;
    // Use theme manager colors directly - works across all themes
    sidebarBg = m_themeManager->secondaryColor().name();
    borderColor = m_themeManager->accentColor().name();
    textColor = m_themeManager->textColor().name();
    iconColor = m_themeManager->textColor();
    QColor accentColor = m_themeManager->accentColor();
    QString hoverColor = accentColor.name();
    QString pressedColor = accentColor.darker(110).name();  // Slight darken for pressed state

    // Special color for sign out button (reddish)
    QString signOutColor = m_themeManager->negativeColor().name();
    QString signOutHoverColor = m_themeManager->negativeColor().lighter(120).name();

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
        QPushButton[class~="signout-button"] {
            color: %6;
        }
        QPushButton[class~="signout-button"]:hover {
            background-color: %7;
            color: white;
        }
        QLabel {
            background-color: transparent;
            color: %5;
        }
        QLabel#signOutText {
            color: inherit;
        }
    )")
                               .arg(sidebarBg)
                               .arg(borderColor)
                               .arg(hoverColor)
                               .arg(pressedColor)
                               .arg(textColor)
                               .arg(signOutColor)
                               .arg(signOutHoverColor);

    setStyleSheet(sidebarStyle);

    // Create colored SVG icons that match text color
    QIcon menuIcon = createColoredIcon(":/icons/icons/menu.svg", iconColor);
    QIcon walletIcon = createColoredIcon(":/icons/icons/wallet.svg", iconColor);
    QIcon chartIcon = createColoredIcon(":/icons/icons/chart.svg", iconColor);
    QIcon settingsIcon = createColoredIcon(":/icons/icons/settings.svg", iconColor);

    // For sign out, use the new logout icon
    QIcon signOutIcon = createColoredIcon(":/icons/icons/logout.svg", QColor(signOutColor));

    // Set icons on buttons
    m_menuButton->setIcon(menuIcon);
    m_menuButton->setIconSize(QSize(24, 24));

    // Set icons on navigation buttons
    QLabel* walletIconLabel = m_walletButton->findChild<QLabel*>("walletIcon");
    QLabel* topCryptosIconLabel = m_topCryptosButton->findChild<QLabel*>("topCryptosIcon");
    QLabel* settingsIconLabel = m_settingsButton->findChild<QLabel*>("settingsIcon");
    QLabel* signOutIconLabel = m_signOutButton->findChild<QLabel*>("signOutIcon");

    if (walletIconLabel) {
        QPixmap walletPixmap = walletIcon.pixmap(QSize(24, 24));
        walletIconLabel->setPixmap(walletPixmap);
    }

    if (topCryptosIconLabel) {
        QPixmap chartPixmap = chartIcon.pixmap(QSize(24, 24));
        topCryptosIconLabel->setPixmap(chartPixmap);
    }

    if (settingsIconLabel) {
        QPixmap settingsPixmap = settingsIcon.pixmap(QSize(24, 24));
        settingsIconLabel->setPixmap(settingsPixmap);
    }

    if (signOutIconLabel) {
        QPixmap signOutPixmap = signOutIcon.pixmap(QSize(24, 24));
        signOutIconLabel->setPixmap(signOutPixmap);
    }

    // Apply fonts to button labels
    QFont textFont = m_themeManager->buttonFont();
    textFont.setPointSize(13);
    textFont.setWeight(QFont::Medium);

    // Apply to wallet button text
    QLabel* walletText = m_walletButton->findChild<QLabel*>("walletText");
    if (walletText)
        walletText->setFont(textFont);

    // Apply to top cryptos button text
    QLabel* topCryptosText = m_topCryptosButton->findChild<QLabel*>("topCryptosText");
    if (topCryptosText)
        topCryptosText->setFont(textFont);

    // Apply to settings button text
    QLabel* settingsText = m_settingsButton->findChild<QLabel*>("settingsText");
    if (settingsText)
        settingsText->setFont(textFont);

    // Apply to sign out button text
    QLabel* signOutText = m_signOutButton->findChild<QLabel*>("signOutText");
    if (signOutText)
        signOutText->setFont(textFont);

    // Force visual refresh
    update();
}
