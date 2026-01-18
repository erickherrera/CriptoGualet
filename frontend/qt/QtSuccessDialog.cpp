#include "include/QtSuccessDialog.h"
#include "QtThemeManager.h"
#include <QGuiApplication>
#include <QScreen>
#include <QStyle>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

QtSuccessDialog::QtSuccessDialog(const QString& username, QWidget* parent)
    : QDialog(parent), m_username(username), m_autoDismissTimer(nullptr) {
    
    setWindowTitle("Registration Complete");
    setModal(true);
    
    // Remove window frame and make it edgeless
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    
    // Resize to entire screen for a fullscreen overlay effect
    if (auto* screen = QGuiApplication::primaryScreen()) {
        this->resize(screen->size());
    } else if (parent) {
        this->resize(parent->size());
    }
    
    // Calculate responsive size based on screen dimensions
    QScreen* screen = QGuiApplication::primaryScreen();
    m_screenGeometry = screen->availableGeometry();
    int screenHeight = m_screenGeometry.height();
    int screenWidth = m_screenGeometry.width();
    
    // Enhanced responsive sizing for ultra-wide and various screen types
    int maxDialogWidth = std::min(480, static_cast<int>(screenWidth * 0.4));
    int maxDialogHeight = std::min(380, static_cast<int>(screenHeight * 0.6));
    
    // Ultra-wide specific handling
    if (screenWidth > 2560) {
        maxDialogWidth = std::min(440, static_cast<int>(screenWidth * 0.25));
    }
    
    // Store dialog dimensions for card positioning
    m_dialogWidth = maxDialogWidth;
    m_dialogHeight = maxDialogHeight;
    m_dialogX = (screenWidth - maxDialogWidth) / 2;
    m_dialogY = (screenHeight - maxDialogHeight) / 2;
    
    setupUI();
    setupConnections();
    applyTheme();
    setupAutoDismiss();
}

QtSuccessDialog::~QtSuccessDialog() {
    if (m_autoDismissTimer) {
        m_autoDismissTimer->stop();
        delete m_autoDismissTimer;
    }
}

void QtSuccessDialog::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Fill background with a semi-transparent color to create a "dimmed" effect
    painter.fillRect(rect(), QColor(0, 0, 0, 150));
}

void QtSuccessDialog::setupUI() {
    // Main container for dialog content, styled as a floating card
    m_card = new QFrame(this);
    m_card->setObjectName("successCard");
    
    // Make card responsive with better minimum size for content, prevent width conflict
    m_card->setMinimumWidth(280); // Adjusted from 340
    m_card->setMinimumHeight(280);
    m_card->setMaximumWidth(m_dialogWidth);
    // Prevent card from growing beyond screen height
    m_card->setMaximumHeight(m_screenGeometry.height());
    
    // Card layout with optimized spacing for content fit
    m_cardLayout = new QVBoxLayout(m_card);
    int margin = std::max(20, std::min(32, static_cast<int>(m_screenGeometry.width() * 0.02)));
    m_cardLayout->setContentsMargins(margin, margin, margin, margin);
    m_cardLayout->setSpacing(12); // Reduced from 16 to 12 for better space
    
    // Success icon with conservative sizing for better fit
    m_successIcon = new QLabel(m_card);
    m_successIcon->setObjectName("successIcon");
    m_successIcon->setAlignment(Qt::AlignCenter);
    m_successIcon->setText("✓");
    
    // Reduced icon size to save space
    int iconSize = std::max(28, std::min(36, static_cast<int>(m_screenGeometry.height() * 0.04)));
    QFont iconFont = m_successIcon->font();
    iconFont.setFamily("Arial");
    iconFont.setPointSize(iconSize);
    iconFont.setBold(true);
    m_successIcon->setFont(iconFont);
    
    // Calculate responsive font sizes - slightly reduced for better fit
    int baseFontSize = std::max(12, std::min(14, static_cast<int>(m_screenGeometry.height() * 0.016)));
    int titleFontSize = std::max(16, std::min(20, static_cast<int>(m_screenGeometry.height() * 0.021)));
    
    // Title with improved styling
    m_titleLabel = new QLabel("Account Created Successfully", m_card);
    m_titleLabel->setObjectName("successTitle");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setWordWrap(true); // Added word wrap for long titles
    QFont titleFont = m_titleLabel->font();
    titleFont.setFamily("Arial");
    titleFont.setPointSize(titleFontSize);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    
    // Message with improved formatting - using proper HTML for spacing
    m_messageLabel = new QLabel(
        QString("Account created for %1<br>Your seed phrase has been securely backed up")
        .arg(m_username), m_card);
    m_messageLabel->setObjectName("successMessage");
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setWordWrap(true);
    QFont messageFont = m_messageLabel->font();
    messageFont.setFamily("Arial");
    messageFont.setPointSize(baseFontSize);
    m_messageLabel->setFont(messageFont);
    
    // Info label with concise message
    m_infoLabel = new QLabel("You can now sign in with your credentials", m_card);
    m_infoLabel->setObjectName("successInfo");
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setWordWrap(true);
    QFont infoFont = m_infoLabel->font();
    infoFont.setFamily("Arial");
    infoFont.setPointSize(baseFontSize);
    m_infoLabel->setFont(infoFont);
    
    // OK button with improved styling
    m_okButton = new QPushButton("OK", m_card);
    m_okButton->setObjectName("successButton");
    
    // Responsive button sizing
    int buttonWidth = std::max(100, std::min(120, static_cast<int>(m_screenGeometry.width() * 0.06)));
    int buttonHeight = std::max(32, std::min(40, static_cast<int>(m_screenGeometry.height() * 0.045)));
    m_okButton->setFixedSize(buttonWidth, buttonHeight);
    
    // Add widgets to card layout with optimized spacing
    m_cardLayout->addWidget(m_successIcon, 0, Qt::AlignCenter);
    m_cardLayout->addWidget(m_titleLabel, 0, Qt::AlignCenter);
    m_cardLayout->addWidget(m_messageLabel, 0, Qt::AlignCenter);
    m_cardLayout->addWidget(m_infoLabel, 0, Qt::AlignCenter);
    
    // Button layout with better alignment
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setContentsMargins(0, 8, 0, 4); // Reduced top/bottom margins
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_okButton);
    m_buttonLayout->addStretch();
    
    m_cardLayout->addLayout(m_buttonLayout);
    
    // The main layout of dialog centers card
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->addWidget(m_card, 0, Qt::AlignCenter);
    setLayout(m_mainLayout);
    
    // Add drop shadow for a floating effect
    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(30);
    shadowEffect->setColor(QColor(0, 0, 0, 100));
    shadowEffect->setOffset(0, 8);
    m_card->setGraphicsEffect(shadowEffect);
}

void QtSuccessDialog::setupConnections() {
    connect(m_okButton, &QPushButton::clicked, this, &QtSuccessDialog::onOkClicked);
}

void QtSuccessDialog::applyTheme() {
    QtThemeManager& tm = QtThemeManager::instance();
    
    // Get theme colors
    QColor bgColor = tm.surfaceColor();
    QColor textColor = tm.textColor();
    QColor primaryColor = tm.primaryColor();
    QColor successColor = QColor(76, 175, 80);
    
    // Dialog background with semi-transparent overlay
    this->setStyleSheet("QDialog { background-color: transparent; }");
    
    // Enhanced card styling with improved appearance
    if (m_card) {
        m_card->setStyleSheet(QString(
            "QFrame#successCard {"
            "   background-color: %1;"
            "   border-radius: 24px;"
            "   border: none;"
            "}"
        ).arg(bgColor.name()));
    }
    
    // Success icon with enhanced green color
    if (m_successIcon) {
        m_successIcon->setStyleSheet(QString(
            "QLabel#successIcon {"
            "   color: %1;"
            "   background-color: transparent;"
            "   padding: 2px;"
            "}"
        ).arg(successColor.name()));
    }
    
    // Title with improved typography
    if (m_titleLabel) {
        m_titleLabel->setStyleSheet(QString(
            "QLabel#successTitle {"
            "   font-size: 18px;"
            "   font-weight: 700;"
            "   color: %1;"
            "   background-color: transparent;"
            "   padding: 0px 0px 4px 0px;"
            "}"
        ).arg(textColor.name()));
    }
    
    // Message with better readability
    if (m_messageLabel) {
        m_messageLabel->setStyleSheet(QString(
            "QLabel#successMessage {"
            "   font-size: 14px;"
            "   color: %1;"
            "   background-color: transparent;"
            "   padding: 4px 0px;"
            "   line-height: 1.3;"
            "}"
        ).arg(textColor.name()));
    }
    
    // Info label with subtle styling
    if (m_infoLabel) {
        m_infoLabel->setStyleSheet(QString(
            "QLabel#successInfo {"
            "   font-size: 13px;"
            "   color: %1;"
            "   background-color: transparent;"
            "   padding: 2px 0px 6px 0px;"
            "}"
        ).arg(tm.subtitleColor().name()));
    }
    
    // Button styling
    if (m_okButton) {
        m_okButton->setStyleSheet(QString(
            "QPushButton#successButton {"
            "   background-color: %1;"
            "   color: white;"
            "   border: none;"
            "   border-radius: 12px;"
            "   font-size: 15px;"
            "   font-weight: 600;"
            "   padding: 10px 24px;"
            "}"
            "QPushButton#successButton:hover {"
            "   background-color: %2;"
            "}"
            "QPushButton#successButton:pressed {"
            "   background-color: %3;"
            "}"
        ).arg(primaryColor.name(),
              primaryColor.darker(110).name(),
              primaryColor.darker(120).name()));
    }
}

void QtSuccessDialog::setupAutoDismiss() {
    m_autoDismissTimer = new QTimer(this);
    m_autoDismissTimer->setSingleShot(true);
    connect(m_autoDismissTimer, &QTimer::timeout, this, &QtSuccessDialog::onAutoDismiss);
    m_autoDismissTimer->start(AUTO_DISMISS_DELAY);
}

void QtSuccessDialog::onOkClicked() {
    if (m_autoDismissTimer) {
        m_autoDismissTimer->stop();
    }
    accept();
}

void QtSuccessDialog::onAutoDismiss() {
    accept();
}

void QtSuccessDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void QtSuccessDialog::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}