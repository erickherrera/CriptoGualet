#include "QtSidebar.h"
#include "QtThemeManager.h"

#include <QTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QSvgRenderer>

QtSidebar::QtSidebar(QtThemeManager* themeManager, QWidget* parent)
    : QWidget(parent),
      m_themeManager(themeManager),
      m_hoverLabel(nullptr),
      m_widthAnimation(nullptr),
      m_hoverFadeAnimation(nullptr),
      m_walletTextOpacity(nullptr),
      m_topCryptosTextOpacity(nullptr),
      m_settingsTextOpacity(nullptr),
      m_signOutTextOpacity(nullptr),
      m_isExpanded(false),
      m_textVisible(false),
      m_wasAnimatingExpand(false),
      m_selectedPage(Page::None) {
    setupUI();
    applyTheme();

    // Set default selected page
    setSelectedPage(Page::Wallet);

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
    m_navLayout->setContentsMargins(14, 24, 14, 24);
    m_navLayout->setSpacing(18);

    // Hamburger menu button at the top
    m_menuButton = new QPushButton(m_sidebarContent);
    m_menuButton->setProperty("class", "sidebar-menu-button");
    m_menuButton->setCursor(Qt::PointingHandCursor);
    m_menuButton->setToolTip("Toggle Menu");
    m_menuButton->setFixedHeight(BUTTON_HEIGHT);

    QHBoxLayout* menuLayout = new QHBoxLayout(m_menuButton);
    // Center icon: button width = 90 - 14*2(nav margins) = 62, so (62 - 32) / 2 = 15
    menuLayout->setContentsMargins(15, 0, 15, 0);
    menuLayout->setSpacing(0);

    QLabel* menuIcon = new QLabel(m_menuButton);
    menuIcon->setObjectName("menuIcon");
    menuIcon->setAlignment(Qt::AlignCenter);
    menuIcon->setFixedSize(ICON_SIZE, ICON_SIZE);
    menuLayout->addWidget(menuIcon, 0, Qt::AlignCenter);

    m_navLayout->addWidget(m_menuButton);

    connect(m_menuButton, &QPushButton::clicked, this, &QtSidebar::toggleSidebar);

    createNavigationButtons();

    // Add stretch to push buttons to top
    m_navLayout->addStretch();

    // Add Sign Out button at the bottom
    m_signOutButton = new QPushButton(m_sidebarContent);
    m_signOutButton->setProperty("class", "sidebar-nav-button signout-button");
    m_signOutButton->setCursor(Qt::PointingHandCursor);
    m_signOutButton->setToolTip("Sign Out");
    m_signOutButton->setFixedHeight(BUTTON_HEIGHT);

    QHBoxLayout* signOutLayout = new QHBoxLayout(m_signOutButton);
    // Center icon: button width = 90 - 14*2(nav margins) = 62, so (62 - 32) / 2 = 15
    signOutLayout->setContentsMargins(15, 0, 15, 0);
    signOutLayout->setSpacing(16);

    QLabel* signOutIcon = new QLabel(m_signOutButton);
    signOutIcon->setObjectName("signOutIcon");
    signOutIcon->setAlignment(Qt::AlignCenter);
    signOutIcon->setFixedSize(ICON_SIZE, ICON_SIZE);
    signOutLayout->addWidget(signOutIcon, 0, Qt::AlignCenter);

    QLabel* signOutText = new QLabel("Sign Out", m_signOutButton);
    signOutText->setObjectName("signOutText");
    signOutText->setVisible(false);
    signOutText->setFixedWidth(100);  // Fixed width to prevent layout recalculations
    signOutLayout->addWidget(signOutText);
    signOutLayout->addStretch();

    m_navLayout->addWidget(m_signOutButton);

    connect(m_signOutButton, &QPushButton::clicked, this, [this]() {
        emit signOutRequested();
    });

    m_mainLayout->addWidget(m_sidebarContent);

    // Create floating hover label for collapsed mode
    m_hoverLabel = new QLabel(this);
    m_hoverLabel->setObjectName("hoverLabel");
    m_hoverLabel->hide();

    // Setup width animation with spring physics (OutBack)
    m_widthAnimation = new QPropertyAnimation(this, "sidebarWidth");
    m_widthAnimation->setDuration(300);
    m_widthAnimation->setEasingCurve(QEasingCurve::OutBack);

    connect(m_widthAnimation, &QPropertyAnimation::finished, this, [this]() {
        setShadowsEnabled(true);
        if (m_isExpanded) {
            m_textVisible = true;
        } else {
            // Safely hide labels only after collapse animation completes
            updateLabelsVisibility(false);
        }
    });

    // Setup hover label fade animation
    m_hoverFadeAnimation = new QPropertyAnimation(this);
    m_hoverFadeAnimation->setDuration(150);
    m_hoverFadeAnimation->setEasingCurve(QEasingCurve::OutQuad);

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
    m_walletButton->setFixedHeight(BUTTON_HEIGHT);

    QHBoxLayout* walletLayout = new QHBoxLayout(m_walletButton);
    // Center icon: button width = 90 - 14*2(nav margins) = 62, so (62 - 32) / 2 = 15
    walletLayout->setContentsMargins(15, 0, 15, 0);
    walletLayout->setSpacing(16);

    QLabel* walletIcon = new QLabel(m_walletButton);
    walletIcon->setObjectName("walletIcon");
    walletIcon->setAlignment(Qt::AlignCenter);
    walletIcon->setFixedSize(ICON_SIZE, ICON_SIZE);
    walletLayout->addWidget(walletIcon, 0, Qt::AlignCenter);

    QLabel* walletText = new QLabel("Wallet", m_walletButton);
    walletText->setObjectName("walletText");
    walletText->setVisible(false);
    walletText->setFixedWidth(100);  // Fixed width to prevent layout recalculations
    walletLayout->addWidget(walletText);
    walletLayout->addStretch();

    m_navLayout->addWidget(m_walletButton);

    connect(m_walletButton, &QPushButton::clicked, this, [this]() {
        setSelectedPage(Page::Wallet);
        emit navigateToWallet();
    });

    // Top Cryptos button with icon and text
    m_topCryptosButton = new QPushButton(m_sidebarContent);
    m_topCryptosButton->setProperty("class", "sidebar-nav-button");
    m_topCryptosButton->setCursor(Qt::PointingHandCursor);
    m_topCryptosButton->setToolTip("Markets");
    m_topCryptosButton->setFixedHeight(BUTTON_HEIGHT);

    QHBoxLayout* topCryptosLayout = new QHBoxLayout(m_topCryptosButton);
    // Center icon: button width = 90 - 14*2(nav margins) = 62, so (62 - 32) / 2 = 15
    topCryptosLayout->setContentsMargins(15, 0, 15, 0);
    topCryptosLayout->setSpacing(16);

    QLabel* topCryptosIcon = new QLabel(m_topCryptosButton);
    topCryptosIcon->setObjectName("topCryptosIcon");
    topCryptosIcon->setAlignment(Qt::AlignCenter);
    topCryptosIcon->setFixedSize(ICON_SIZE, ICON_SIZE);
    topCryptosLayout->addWidget(topCryptosIcon, 0, Qt::AlignCenter);

    QLabel* topCryptosText = new QLabel("Markets", m_topCryptosButton);
    topCryptosText->setObjectName("topCryptosText");
    topCryptosText->setVisible(false);
    topCryptosText->setFixedWidth(100);  // Fixed width to prevent layout recalculations
    topCryptosLayout->addWidget(topCryptosText);
    topCryptosLayout->addStretch();

    m_navLayout->addWidget(m_topCryptosButton);

    connect(m_topCryptosButton, &QPushButton::clicked, this, [this]() {
        setSelectedPage(Page::TopCryptos);
        emit navigateToTopCryptos();
    });

    // Settings button with icon and text
    m_settingsButton = new QPushButton(m_sidebarContent);
    m_settingsButton->setProperty("class", "sidebar-nav-button");
    m_settingsButton->setCursor(Qt::PointingHandCursor);
    m_settingsButton->setToolTip("Settings");
    m_settingsButton->setFixedHeight(BUTTON_HEIGHT);

    QHBoxLayout* settingsLayout = new QHBoxLayout(m_settingsButton);
    // Center icon: button width = 90 - 14*2(nav margins) = 62, so (62 - 32) / 2 = 15
    settingsLayout->setContentsMargins(15, 0, 15, 0);
    settingsLayout->setSpacing(16);

    QLabel* settingsIcon = new QLabel(m_settingsButton);
    settingsIcon->setObjectName("settingsIcon");
    settingsIcon->setAlignment(Qt::AlignCenter);
    settingsIcon->setFixedSize(ICON_SIZE, ICON_SIZE);
    settingsLayout->addWidget(settingsIcon, 0, Qt::AlignCenter);

    QLabel* settingsText = new QLabel("Settings", m_settingsButton);
    settingsText->setObjectName("settingsText");
    settingsText->setVisible(false);
    settingsText->setFixedWidth(100);  // Fixed width to prevent layout recalculations
    settingsLayout->addWidget(settingsText);
    settingsLayout->addStretch();

    m_navLayout->addWidget(m_settingsButton);

    connect(m_settingsButton, &QPushButton::clicked, this, [this]() {
        setSelectedPage(Page::Settings);
        emit navigateToSettings();
    });
}

void QtSidebar::setSelectedPage(Page page) {
    m_selectedPage = page;
    m_walletButton->setProperty("selected", page == Page::Wallet);
    m_topCryptosButton->setProperty("selected", page == Page::TopCryptos);
    m_settingsButton->setProperty("selected", page == Page::Settings);

    // Update styles to reflect the change
    auto updateButtonStyle = [](QPushButton* btn) {
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
    };

    updateButtonStyle(m_walletButton);
    updateButtonStyle(m_topCryptosButton);
    updateButtonStyle(m_settingsButton);

    // Update icon and text colors based on selection using cache
    auto updateIconAndText = [this](QPushButton* btn, bool selected, Page pageType,
                                    const QString& textName, const QString& iconName) {
        QLabel* iconLabel = btn->findChild<QLabel*>(iconName);
        QLabel* textLabel = btn->findChild<QLabel*>(textName);
        QColor color = selected ? Qt::white : m_themeManager->textColor();

        if (iconLabel && m_iconCache.contains(pageType)) {
            const auto& icons = m_iconCache[pageType];
            iconLabel->setPixmap(selected ? icons.active : icons.inactive);
        }
        if (textLabel) {
            textLabel->setStyleSheet(QString("color: %1;").arg(color.name()));
        }
    };

    updateIconAndText(m_walletButton, page == Page::Wallet, Page::Wallet, "walletText",
                      "walletIcon");
    updateIconAndText(m_topCryptosButton, page == Page::TopCryptos, Page::TopCryptos,
                      "topCryptosText", "topCryptosIcon");
    updateIconAndText(m_settingsButton, page == Page::Settings, Page::Settings, "settingsText",
                      "settingsIcon");

    update();
}

void QtSidebar::toggleSidebar() {
    m_isExpanded = !m_isExpanded;
    animateSidebar(m_isExpanded);
}

void QtSidebar::animateSidebar(bool expand) {
    int targetWidth = expand ? EXPANDED_WIDTH : COLLAPSED_WIDTH;
    int startWidth = width();
    int totalDistance = EXPANDED_WIDTH - COLLAPSED_WIDTH;

    // Stop any running text opacity animations to prevent overlap/flickering
    stopActiveTextAnimations();

    // Calculate remaining distance fraction for consistent animation speed
    qreal remainingFraction = 1.0;
    if (m_widthAnimation->state() == QAbstractAnimation::Running) {
        m_widthAnimation->stop();
        // Calculate fraction based purely on pixel distance remaining
        int distanceToTarget = qAbs(targetWidth - startWidth);
        remainingFraction = static_cast<qreal>(distanceToTarget) / totalDistance;
    }

    // Disable shadows during animation for performance
    setShadowsEnabled(false);

    // Handle text visibility with staggered timing
    if (!expand) {
        // Fade out text, then hide labels when animation finishes
        animateTextOpacity(0.0, TEXT_HIDE_ADVANCE, 0);
        m_textVisible = false;
    }

    // Adjust duration proportionally to remaining distance for consistent speed
    int adjustedDuration = static_cast<int>(300 * remainingFraction);
    m_widthAnimation->setDuration(qMax(adjustedDuration, 100));

    m_widthAnimation->setStartValue(startWidth);
    m_widthAnimation->setEndValue(targetWidth);

    // Hide hover label immediately when animating
    hideHoverLabel();

    // Track direction for interruption handling
    m_wasAnimatingExpand = expand;

    m_widthAnimation->start();

    // For expansion, stagger text appearance after width animation starts
    if (expand) {
        updateLabelsVisibility(true);
        animateTextOpacity(0.0, 0, 0);  // Start at invisible
        animateTextOpacity(1.0, 200, TEXT_STAGGER_DELAY);
    }
}

bool QtSidebar::eventFilter(QObject* obj, QEvent* event) {
    // Ignore interaction during animation to prevent stutter
    if (m_widthAnimation && m_widthAnimation->state() == QAbstractAnimation::Running) {
        return QWidget::eventFilter(obj, event);
    }

    // Handle hover labels when collapsed
    if (!m_isExpanded) {
        if (obj == m_walletButton || obj == m_topCryptosButton || obj == m_settingsButton ||
            obj == m_signOutButton) {
            if (event->type() == QEvent::Enter) {
                QString text;
                bool isSignOut = false;
                if (obj == m_walletButton)
                    text = "Wallet";
                else if (obj == m_topCryptosButton)
                    text = "Markets";
                else if (obj == m_settingsButton)
                    text = "Settings";
                else if (obj == m_signOutButton) {
                    text = "Sign Out";
                    isSignOut = true;
                }
                QPushButton* btn = qobject_cast<QPushButton*>(obj);
                if (btn) {
                    showHoverLabel(text, btn->y() + btn->height() / 2, isSignOut);
                }
            } else if (event->type() == QEvent::Leave) {
                hideHoverLabel();
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

QIcon QtSidebar::createColoredIcon(const QString& svgPath, const QColor& color) {
    QSvgRenderer renderer(svgPath);
    QPixmap pixmap(ICON_SIZE, ICON_SIZE);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    renderer.render(&painter);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);
    painter.end();

    return QIcon(pixmap);
}

void QtSidebar::applyTheme() {
    QString sidebarBg, borderColor, textColor;
    QColor iconColor;
    sidebarBg = m_themeManager->secondaryColor().name();
    borderColor = m_themeManager->accentColor().name();
    textColor = m_themeManager->textColor().name();
    iconColor = m_themeManager->textColor();
    QColor accentColor = m_themeManager->accentColor();

    QString hoverColor = m_themeManager->lightInfo().name();
    QString pressedColor = accentColor.darker(110).name();
    QString signOutColor = m_themeManager->negativeColor().name();
    QString accentColorName = accentColor.name();

    QString sidebarStyle = QString(R"(
        QtSidebar {
            background-color: %1;
            border-right: 3px solid %2;
        }
        QPushButton[class="sidebar-menu-button"] {
            background-color: %1;
            border: none;
            border-radius: 12px;
            padding: 0px;
            text-align: center;
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
            background-color: %7;
        }
        QPushButton[class="sidebar-nav-button"]:pressed {
            background-color: %4;
        }
        QPushButton[class="sidebar-nav-button"][selected="true"] {
            background-color: %3;
            color: white;
        }
        QPushButton[class~="signout-button"] {
            background-color: %3;
            color: white;
            border: none;
            border-radius: 12px;
        }
        QPushButton[class~="signout-button"]:hover {
            background-color: %7;
            color: %6;
        }
        QLabel {
            background-color: transparent;
            color: %5;
        }
        QLabel#signOutText {
            color: inherit;
        }
        QLabel#hoverLabel {
            background-color: %1;
            color: %5;
            padding: 6px 12px;
            border-radius: 6px;
            border: 1px solid %2;
        }
    )")
                               .arg(sidebarBg)
                               .arg(borderColor)
                               .arg(accentColorName)
                               .arg(pressedColor)
                               .arg(textColor)
                               .arg(signOutColor)
                               .arg(hoverColor)
                               .arg(accentColorName);

    setStyleSheet(sidebarStyle);

    cacheIcons();

    QIcon menuIcon = createColoredIcon(":/icons/icons/menu.svg", textColor);
    QIcon signOutIcon = createColoredIcon(":/icons/icons/logout.svg", QColor(signOutColor));

    // Update menu icon
    QLabel* menuIconLabel = m_menuButton->findChild<QLabel*>("menuIcon");
    if (menuIconLabel) {
        menuIconLabel->setPixmap(menuIcon.pixmap(ICON_SIZE, ICON_SIZE));
    } else {
        // Fallback if layout not ready (should not happen with new logic)
        m_menuButton->setIcon(menuIcon);
        m_menuButton->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
    }

    QLabel* signOutIconLabel = m_signOutButton->findChild<QLabel*>("signOutIcon");
    if (signOutIconLabel) {
        QPixmap signOutPixmap = signOutIcon.pixmap(QSize(ICON_SIZE, ICON_SIZE));
        signOutIconLabel->setPixmap(signOutPixmap);
    }

    QFont textFont = m_themeManager->buttonFont();
    textFont.setPointSize(14);
    textFont.setWeight(QFont::Medium);

    QLabel* walletText = m_walletButton->findChild<QLabel*>("walletText");
    if (walletText)
        walletText->setFont(textFont);

    QLabel* topCryptosText = m_topCryptosButton->findChild<QLabel*>("topCryptosText");
    if (topCryptosText)
        topCryptosText->setFont(textFont);

    QLabel* settingsText = m_settingsButton->findChild<QLabel*>("settingsText");
    if (settingsText)
        settingsText->setFont(textFont);

    QLabel* signOutText = m_signOutButton->findChild<QLabel*>("signOutText");
    if (signOutText)
        signOutText->setFont(textFont);

    update();

    if (m_selectedPage != Page::None) {
        setSelectedPage(m_selectedPage);
    }
}

void QtSidebar::showHoverLabel(const QString& text, int yPos, bool isSignOut) {
    if (!m_hoverLabel)
        return;

    m_hoverLabel->setText(text);
    m_hoverLabel->adjustSize();

    if (isSignOut) {
        QString accentColor = m_themeManager->accentColor().name();
        m_hoverLabel->setStyleSheet(
            QString("background-color: %1; color: white; padding: 6px 12px; "
                    "border-radius: 6px; border: 1px solid %2;")
                .arg(accentColor)
                .arg(accentColor));
    } else {
        m_hoverLabel->setStyleSheet(QString("background-color: %1; color: %2; padding: 6px 12px; "
                                            "border-radius: 6px; border: 1px solid %3;")
                                        .arg(m_themeManager->secondaryColor().name())
                                        .arg(m_themeManager->textColor().name())
                                        .arg(m_themeManager->accentColor().name()));
    }

    int labelX = width() + 8;
    int labelY = yPos - m_hoverLabel->height() / 2;
    m_hoverLabel->move(labelX, labelY);
    m_hoverLabel->raise();

    // Animate opacity for smooth fade-in
    animateHoverLabelOpacity(1.0);
}

void QtSidebar::animateHoverLabelOpacity(qreal targetOpacity) {
    if (!m_hoverLabel)
        return;

    // Setup opacity effect if not already set
    QGraphicsOpacityEffect* opacityEffect =
        qobject_cast<QGraphicsOpacityEffect*>(m_hoverLabel->graphicsEffect());
    if (!opacityEffect) {
        opacityEffect = new QGraphicsOpacityEffect(m_hoverLabel);
        m_hoverLabel->setGraphicsEffect(opacityEffect);
    }

    if (targetOpacity > 0) {
        opacityEffect->setOpacity(0.0);
        m_hoverLabel->show();
    }

    if (m_hoverFadeAnimation->state() == QAbstractAnimation::Running) {
        m_hoverFadeAnimation->stop();
    }

    // Disconnect previous finished handler precisely
    if (m_hoverFadeFinishedConn) {
        disconnect(m_hoverFadeFinishedConn);
    }

    m_hoverFadeAnimation->setTargetObject(opacityEffect);
    m_hoverFadeAnimation->setPropertyName("opacity");
    m_hoverFadeAnimation->setStartValue(opacityEffect->opacity());
    m_hoverFadeAnimation->setEndValue(targetOpacity);
    m_hoverFadeAnimation->start();

    if (targetOpacity == 0.0) {
        m_hoverFadeFinishedConn =
            connect(m_hoverFadeAnimation, &QPropertyAnimation::finished, this, [this]() {
                if (m_hoverLabel)
                    m_hoverLabel->hide();
            });
    }
}

void QtSidebar::hideHoverLabel() {
    if (m_hoverLabel) {
        // Use fade out animation instead of immediate hide
        animateHoverLabelOpacity(0.0);
    }
}

void QtSidebar::updateLabelsVisibility(bool visible) {
    QLabel* walletText = m_walletButton->findChild<QLabel*>("walletText");
    QLabel* topCryptosText = m_topCryptosButton->findChild<QLabel*>("topCryptosText");
    QLabel* settingsText = m_settingsButton->findChild<QLabel*>("settingsText");
    QLabel* signOutText = m_signOutButton->findChild<QLabel*>("signOutText");

    if (walletText) {
        walletText->setVisible(visible);
        if (!m_walletTextOpacity) {
            m_walletTextOpacity = new QGraphicsOpacityEffect(walletText);
            walletText->setGraphicsEffect(m_walletTextOpacity);
        }
    }
    if (topCryptosText) {
        topCryptosText->setVisible(visible);
        if (!m_topCryptosTextOpacity) {
            m_topCryptosTextOpacity = new QGraphicsOpacityEffect(topCryptosText);
            topCryptosText->setGraphicsEffect(m_topCryptosTextOpacity);
        }
    }
    if (settingsText) {
        settingsText->setVisible(visible);
        if (!m_settingsTextOpacity) {
            m_settingsTextOpacity = new QGraphicsOpacityEffect(settingsText);
            settingsText->setGraphicsEffect(m_settingsTextOpacity);
        }
    }
    if (signOutText) {
        signOutText->setVisible(visible);
        if (!m_signOutTextOpacity) {
            m_signOutTextOpacity = new QGraphicsOpacityEffect(signOutText);
            signOutText->setGraphicsEffect(m_signOutTextOpacity);
        }
    }
}

void QtSidebar::animateTextOpacity(qreal targetOpacity, int duration, int delay) {
    if (delay > 0) {
        QTimer::singleShot(delay, this, [this, targetOpacity, duration]() {
            animateTextOpacity(targetOpacity, duration, 0);
        });
        return;
    }

    if (m_walletTextOpacity) {
        createOpacityAnimation(m_walletTextOpacity, targetOpacity, duration, 0);
    }
    if (m_topCryptosTextOpacity) {
        createOpacityAnimation(m_topCryptosTextOpacity, targetOpacity, duration,
                               TEXT_STAGGER_DELAY / 2);
    }
    if (m_settingsTextOpacity) {
        createOpacityAnimation(m_settingsTextOpacity, targetOpacity, duration, TEXT_STAGGER_DELAY);
    }
    if (m_signOutTextOpacity) {
        createOpacityAnimation(m_signOutTextOpacity, targetOpacity, duration,
                               TEXT_STAGGER_DELAY + 20);
    }
}

void QtSidebar::stopActiveTextAnimations() {
    for (QPropertyAnimation* anim : m_activeTextAnimations) {
        if (anim && anim->state() == QAbstractAnimation::Running) {
            anim->stop();
        }
    }
    m_activeTextAnimations.clear();
}

void QtSidebar::createOpacityAnimation(QGraphicsOpacityEffect* effect, qreal targetOpacity,
                                       int duration, int delay) {
    if (!effect)
        return;

    QPropertyAnimation* anim = new QPropertyAnimation(effect, "opacity");
    anim->setDuration(duration);
    anim->setStartValue(effect->opacity());
    anim->setEndValue(targetOpacity);
    anim->setEasingCurve(QEasingCurve::OutQuad);

    // Track this animation so it can be stopped on interruption
    m_activeTextAnimations.append(anim);
    connect(anim, &QPropertyAnimation::finished, this, [this, anim]() {
        m_activeTextAnimations.removeOne(anim);
    });
    connect(anim, &QObject::destroyed, this, [this, anim]() {
        m_activeTextAnimations.removeOne(anim);
    });

    if (delay > 0) {
        QTimer::singleShot(delay, this, [anim]() {
            if (anim)
                anim->start(QAbstractAnimation::DeleteWhenStopped);
        });
    } else {
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
}

void QtSidebar::setSidebarWidth(int width) {
    setFixedWidth(width);
}

void QtSidebar::setShadowsEnabled(bool enabled) {
    // No-op - using CSS box-shadow instead for better performance
    Q_UNUSED(enabled);
}

void QtSidebar::cacheIcons() {
    QColor activeColor = Qt::white;
    QColor inactiveColor = m_themeManager->textColor();

    auto cacheForPage = [&](Page page, const QString& path) {
        m_iconCache[page] = {createColoredIcon(path, activeColor).pixmap(ICON_SIZE, ICON_SIZE),
                             createColoredIcon(path, inactiveColor).pixmap(ICON_SIZE, ICON_SIZE)};
    };

    cacheForPage(Page::Wallet, ":/icons/icons/wallet.svg");
    cacheForPage(Page::TopCryptos, ":/icons/icons/chart.svg");
    cacheForPage(Page::Settings, ":/icons/icons/settings.svg");
}
