#include "include/QtTopCryptosPage.h"
#include "include/QtThemeManager.h"
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QMessageBox>
#include <QSpacerItem>
#include <QTimer>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPixmap>

// ============================================================================
// QtCryptoCard Implementation
// ============================================================================

QtCryptoCard::QtCryptoCard(QWidget *parent)
    : QFrame(parent)
    , m_themeManager(&QtThemeManager::instance())
    , m_networkManager(new QNetworkAccessManager(this)) {
    setupUI();
    applyTheme();

    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &QtCryptoCard::onIconDownloaded);
}

void QtCryptoCard::setupUI() {
    // Main layout
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(16);

    // Crypto icon (left side)
    m_iconLabel = new QLabel(this);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setFixedSize(48, 48);
    m_iconLabel->setScaledContents(true);

    // Crypto info (middle)
    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);

    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    m_symbolLabel = new QLabel(this);
    m_symbolLabel->setStyleSheet("font-size: 20px; font-weight: bold;");

    m_nameLabel = new QLabel(this);
    m_nameLabel->setStyleSheet("font-size: 14px; opacity: 0.7;");

    topRow->addWidget(m_symbolLabel);
    topRow->addWidget(m_nameLabel);
    topRow->addStretch();

    m_marketCapLabel = new QLabel(this);
    m_marketCapLabel->setStyleSheet("font-size: 12px; opacity: 0.6;");

    infoLayout->addLayout(topRow);
    infoLayout->addWidget(m_marketCapLabel);

    // Price info (right side)
    QVBoxLayout *priceLayout = new QVBoxLayout();
    priceLayout->setSpacing(4);
    priceLayout->setAlignment(Qt::AlignRight);

    m_priceLabel = new QLabel(this);
    m_priceLabel->setAlignment(Qt::AlignRight);
    m_priceLabel->setStyleSheet("font-size: 20px; font-weight: 600;");

    m_changeLabel = new QLabel(this);
    m_changeLabel->setAlignment(Qt::AlignRight);
    m_changeLabel->setStyleSheet(
        "font-size: 14px;"
        "font-weight: 600;"
        "padding: 4px 12px;"
        "border-radius: 12px;"
    );

    priceLayout->addWidget(m_priceLabel);
    priceLayout->addWidget(m_changeLabel);

    // Add to main layout
    mainLayout->addWidget(m_iconLabel);
    mainLayout->addLayout(infoLayout, 1);
    mainLayout->addLayout(priceLayout);

    // Card styling
    setFrameShape(QFrame::NoFrame);
    setMinimumHeight(100);
}

void QtCryptoCard::setCryptoData(const PriceService::CryptoPriceData &data, int rank) {
    // Download crypto icon
    QString symbol = QString::fromStdString(data.symbol);
    QString iconUrl = getCryptoIconUrl(symbol);

    // Set placeholder while loading
    m_iconLabel->clear();
    m_iconLabel->setStyleSheet("border-radius: 24px; background-color: rgba(100, 116, 139, 0.1);");

    // Trigger icon download with proper headers
    QNetworkRequest request(iconUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "CriptoGualet/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_networkManager->get(request);

    // Add rank prefix to symbol
    m_symbolLabel->setText(QString("#%1  %2")
        .arg(rank)
        .arg(symbol));
    m_nameLabel->setText(QString::fromStdString(data.name));
    m_priceLabel->setText(formatPrice(data.usd_price));
    m_marketCapLabel->setText("MCap: " + formatMarketCap(data.market_cap));

    // Format price change with color
    QString changeText = QString("%1%2")
        .arg(data.price_change_24h >= 0 ? "+" : "")
        .arg(data.price_change_24h, 0, 'f', 2) + "%";
    m_changeLabel->setText(changeText);

    // Apply color based on change
    if (data.price_change_24h >= 0) {
        m_changeLabel->setStyleSheet(
            "font-size: 14px;"
            "font-weight: 600;"
            "padding: 4px 12px;"
            "border-radius: 12px;"
            "background-color: rgba(34, 197, 94, 0.15);"
            "color: #22c55e;"
        );
    } else {
        m_changeLabel->setStyleSheet(
            "font-size: 14px;"
            "font-weight: 600;"
            "padding: 4px 12px;"
            "border-radius: 12px;"
            "background-color: rgba(239, 68, 68, 0.15);"
            "color: #ef4444;"
        );
    }
}

QString QtCryptoCard::formatPrice(double price) {
    if (price >= 1000) {
        return QString("$%L1").arg(price, 0, 'f', 2);
    } else if (price >= 1) {
        return QString("$%1").arg(price, 0, 'f', 2);
    } else {
        return QString("$%1").arg(price, 0, 'f', 6);
    }
}

QString QtCryptoCard::formatMarketCap(double marketCap) {
    if (marketCap >= 1e12) {
        return QString("$%1T").arg(marketCap / 1e12, 0, 'f', 2);
    } else if (marketCap >= 1e9) {
        return QString("$%1B").arg(marketCap / 1e9, 0, 'f', 2);
    } else if (marketCap >= 1e6) {
        return QString("$%1M").arg(marketCap / 1e6, 0, 'f', 2);
    } else {
        return QString("$%L1").arg(marketCap, 0, 'f', 0);
    }
}

QString QtCryptoCard::getCryptoIconUrl(const QString &symbol) {
    // Use CoinGecko assets API
    // Format: https://assets.coingecko.com/coins/images/{id}/large/{coin}.png
    static const QMap<QString, QString> symbolToImagePath = {
        {"BTC", "1/large/bitcoin.png"},
        {"ETH", "279/large/ethereum.png"},
        {"USDT", "325/large/tether.png"},
        {"BNB", "825/large/binance-coin-logo.png"},
        {"SOL", "4128/large/solana.png"},
        {"USDC", "6319/large/usd-coin.png"},
        {"XRP", "44/large/xrp.png"},
        {"DOGE", "5/large/dogecoin.png"},
        {"ADA", "975/large/cardano.png"},
        {"TRX", "1094/large/tron-logo.png"},
        {"AVAX", "12559/large/Avalanche_Circle_RedWhite_Trans.png"},
        {"SHIB", "11939/large/shiba.png"},
        {"DOT", "12171/large/polkadot.png"},
        {"LINK", "877/large/chainlink-new-logo.png"},
        {"MATIC", "4713/large/matic-token-icon.png"}
    };

    QString imagePath = symbolToImagePath.value(symbol.toUpper(), "1/large/bitcoin.png");
    return QString("https://assets.coingecko.com/coins/images/%1").arg(imagePath);
}

void QtCryptoCard::onIconDownloaded(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray imageData = reply->readAll();
        QPixmap pixmap;

        if (pixmap.loadFromData(imageData)) {
            // Scale pixmap to fit icon label with smooth transformation
            QPixmap scaledPixmap = pixmap.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);

            // Create a rounded pixmap
            QPixmap roundedPixmap(48, 48);
            roundedPixmap.fill(Qt::transparent);

            QPainter painter(&roundedPixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);

            QPainterPath path;
            path.addEllipse(0, 0, 48, 48);
            painter.setClipPath(path);

            // Center the scaled pixmap
            int x = (48 - scaledPixmap.width()) / 2;
            int y = (48 - scaledPixmap.height()) / 2;
            painter.drawPixmap(x, y, scaledPixmap);

            m_iconLabel->setPixmap(roundedPixmap);
            m_iconLabel->setStyleSheet("");

            qDebug() << "Successfully loaded crypto icon from" << reply->url().toString();
        } else {
            qDebug() << "Failed to load image data from" << reply->url().toString();
            setFallbackIcon();
        }
    } else {
        qDebug() << "Network error downloading crypto icon:" << reply->errorString()
                 << "from" << reply->url().toString();
        setFallbackIcon();
    }
    reply->deleteLater();
}

void QtCryptoCard::setFallbackIcon() {
    // Create a simple colored circle as fallback
    QPixmap fallback(48, 48);
    fallback.fill(Qt::transparent);

    QPainter painter(&fallback);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw colored circle
    painter.setBrush(QColor(100, 116, 139, 50));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, 48, 48);

    // Draw currency symbol
    painter.setPen(QColor(100, 116, 139));
    QFont font = painter.font();
    font.setPointSize(18);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRect(0, 0, 48, 48), Qt::AlignCenter, "$");

    m_iconLabel->setPixmap(fallback);
    m_iconLabel->setStyleSheet("");
}

void QtCryptoCard::applyTheme() {
    QString cardBg = m_themeManager->surfaceColor().name();

    QString cardStyle = QString(
        "QFrame {"
        "  background-color: %1;"
        "  border-radius: 16px;"
        "  border: none;"
        "}"
    ).arg(cardBg);

    setStyleSheet(cardStyle);

    // Add subtle shadow effect
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setXOffset(0);
    shadow->setYOffset(4);
    shadow->setColor(QColor(0, 0, 0, 30));
    setGraphicsEffect(shadow);
}

// ============================================================================
// QtTopCryptosPage Implementation
// ============================================================================

QtTopCryptosPage::QtTopCryptosPage(QWidget *parent)
    : QWidget(parent)
    , m_themeManager(&QtThemeManager::instance())
    , m_priceFetcher(std::make_unique<PriceService::PriceFetcher>(15))
    , m_refreshTimer(new QTimer(this)) {

    setupUI();
    applyTheme();

    // Connect to theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtTopCryptosPage::applyTheme);

    // Connect refresh timer (auto-refresh every 60 seconds)
    connect(m_refreshTimer, &QTimer::timeout, this, &QtTopCryptosPage::onAutoRefreshTimer);
    m_refreshTimer->start(60000);

    // Initial data fetch
    fetchTopCryptos();
}

void QtTopCryptosPage::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);
    m_mainLayout->setSpacing(0);

    // Create a horizontal layout to center content with max width
    QHBoxLayout *centeringLayout = new QHBoxLayout();

    // Add left spacer
    centeringLayout->addItem(
        new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    // Create scroll area for all content including header
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    m_scrollContent = new QWidget();
    m_contentLayout = new QVBoxLayout(m_scrollContent);
    m_contentLayout->setContentsMargins(32, 24, 32, 24);
    m_contentLayout->setSpacing(16);

    // Create header inside scroll area
    createHeader();
    m_contentLayout->addWidget(m_headerWidget);

    // Add spacing after header
    m_contentLayout->addSpacing(8);

    // Cards container
    m_cardsContainer = new QWidget();
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(12);

    m_contentLayout->addWidget(m_cardsContainer);
    m_contentLayout->addStretch();

    m_scrollArea->setWidget(m_scrollContent);

    // Add scroll area to centering layout
    centeringLayout->addWidget(m_scrollArea);

    // Add right spacer
    centeringLayout->addItem(
        new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

    m_mainLayout->addLayout(centeringLayout);

    // Create initial empty cards
    createCryptoCards();

    // Initialize responsive layout
    QTimer::singleShot(0, this, [this]() {
        updateScrollAreaWidth();
    });
}

void QtTopCryptosPage::createHeader() {
    m_headerWidget = new QWidget();
    QVBoxLayout *headerLayout = new QVBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 16);
    headerLayout->setSpacing(12);

    // Top row: Back button and refresh button
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    m_backButton = new QPushButton("← Back", m_headerWidget);
    m_backButton->setFixedHeight(40);
    m_backButton->setCursor(Qt::PointingHandCursor);
    connect(m_backButton, &QPushButton::clicked, this, &QtTopCryptosPage::onBackClicked);

    topRow->addWidget(m_backButton);
    topRow->addStretch();

    m_refreshButton = new QPushButton("⟳ Refresh", m_headerWidget);
    m_refreshButton->setFixedHeight(40);
    m_refreshButton->setCursor(Qt::PointingHandCursor);
    connect(m_refreshButton, &QPushButton::clicked, this, &QtTopCryptosPage::onRefreshClicked);

    topRow->addWidget(m_refreshButton);

    // Title section
    QVBoxLayout *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    m_titleLabel = new QLabel("Top Cryptocurrencies", m_headerWidget);
    m_titleLabel->setProperty("class", "title");

    m_subtitleLabel = new QLabel("Live prices updated every 60 seconds", m_headerWidget);
    m_subtitleLabel->setProperty("class", "subtitle");

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);

    headerLayout->addLayout(topRow);
    headerLayout->addLayout(titleLayout);
}

void QtTopCryptosPage::createCryptoCards() {
    // Clear existing cards
    for (auto *card : m_cryptoCards) {
        card->deleteLater();
    }
    m_cryptoCards.clear();

    qDebug() << "Creating 5 crypto card widgets...";

    // Create 5 cards
    for (int i = 0; i < 5; ++i) {
        QtCryptoCard *card = new QtCryptoCard(m_cardsContainer);
        card->setMinimumHeight(100);
        card->setVisible(false); // Hide until we have data
        m_cryptoCards.append(card);
        m_cardsLayout->addWidget(card);
        qDebug() << "Created card" << i;
    }
}

void QtTopCryptosPage::fetchTopCryptos() {
    qDebug() << "Fetching top 5 cryptocurrencies...";

    // Show loading state
    m_subtitleLabel->setText("Loading cryptocurrency data...");

    // Fetch data (this is a blocking call)
    m_cryptoData = m_priceFetcher->GetTopCryptosByMarketCap(5);

    qDebug() << "Fetch complete. Retrieved" << m_cryptoData.size() << "items";

    updateCards();
}

void QtTopCryptosPage::updateCards() {
    if (m_cryptoData.empty()) {
        // Show error or loading state
        qDebug() << "No crypto data available - API call may have failed";

        // Update subtitle to show error
        m_subtitleLabel->setText("Failed to load data. Click refresh to try again.");
        QString errorColor = m_themeManager->errorColor().name();
        QFont errorFont = m_themeManager->textFont();
        errorFont.setPointSize(12);
        errorFont.setBold(true);
        m_subtitleLabel->setFont(errorFont);
        m_subtitleLabel->setStyleSheet(QString("color: %1;").arg(errorColor));

        // Hide all cards
        for (auto *card : m_cryptoCards) {
            card->setVisible(false);
        }
        return;
    }

    qDebug() << "Successfully loaded" << m_cryptoData.size() << "cryptocurrencies";

    // Reset subtitle
    m_subtitleLabel->setText("Live prices updated every 60 seconds");
    QString subtitleColor = m_themeManager->subtitleColor().name();
    QFont subtitleFont = m_themeManager->textFont();
    subtitleFont.setPointSize(12);
    m_subtitleLabel->setFont(subtitleFont);
    m_subtitleLabel->setStyleSheet(QString("color: %1;").arg(subtitleColor));

    int rank = 1;
    for (size_t i = 0; i < m_cryptoData.size() && i < m_cryptoCards.size(); ++i) {
        qDebug() << "Setting card" << i << ":" << QString::fromStdString(m_cryptoData[i].name);
        m_cryptoCards[i]->setCryptoData(m_cryptoData[i], rank++);
        m_cryptoCards[i]->setVisible(true);
    }

    // Hide unused cards
    for (size_t i = m_cryptoData.size(); i < m_cryptoCards.size(); ++i) {
        m_cryptoCards[i]->setVisible(false);
    }
}

void QtTopCryptosPage::applyTheme() {
    QString bgColor = m_themeManager->backgroundColor().name();
    QString txtColor = m_themeManager->textColor().name();
    QString accentColor = m_themeManager->accentColor().name();
    QString surfaceColor = m_themeManager->surfaceColor().name();
    QString subtitleColor = m_themeManager->subtitleColor().name();

    // Page-level styling
    QString pageStyle = QString(
        "QWidget {"
        "  background-color: %1;"
        "  color: %2;"
        "}"
        "QScrollArea {"
        "  background-color: %1;"
        "  border: none;"
        "}"
    ).arg(bgColor, txtColor);

    setStyleSheet(pageStyle);

    // Apply theme to all cards
    for (auto *card : m_cryptoCards) {
        card->applyTheme();
    }

    // Back button styling
    QString backButtonStyle = QString(
        "QPushButton {"
        "  background-color: transparent;"
        "  border: none;"
        "  font-size: 16px;"
        "  font-weight: 600;"
        "  padding: 8px 16px;"
        "  text-align: left;"
        "  color: %1;"
        "}"
        "QPushButton:hover {"
        "  background-color: %2;"
        "  border-radius: 8px;"
        "}"
    ).arg(txtColor, surfaceColor);
    m_backButton->setStyleSheet(backButtonStyle);

    // Refresh button styling
    QString refreshButtonStyle = QString(
        "QPushButton {"
        "  background-color: rgba(%1, %2, %3, 0.1);"
        "  border: 2px solid %4;"
        "  border-radius: 8px;"
        "  color: %4;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "  padding: 8px 20px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(%1, %2, %3, 0.2);"
        "}"
        "QPushButton:pressed {"
        "  background-color: rgba(%1, %2, %3, 0.3);"
        "}"
        "QPushButton:disabled {"
        "  opacity: 0.5;"
        "}"
    ).arg(m_themeManager->accentColor().red())
     .arg(m_themeManager->accentColor().green())
     .arg(m_themeManager->accentColor().blue())
     .arg(accentColor);
    m_refreshButton->setStyleSheet(refreshButtonStyle);

    // Title styling
    QFont titleFont = m_themeManager->titleFont();
    titleFont.setPointSize(24);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(QString("color: %1;").arg(txtColor));

    // Subtitle styling
    QFont subtitleFont = m_themeManager->textFont();
    subtitleFont.setPointSize(12);
    m_subtitleLabel->setFont(subtitleFont);
    m_subtitleLabel->setStyleSheet(QString("color: %1;").arg(subtitleColor));
}

void QtTopCryptosPage::refreshData() {
    fetchTopCryptos();
}

void QtTopCryptosPage::onRefreshClicked() {
    m_refreshButton->setText("⟳ Refreshing...");
    m_refreshButton->setEnabled(false);

    fetchTopCryptos();

    m_refreshButton->setText("⟳ Refresh");
    m_refreshButton->setEnabled(true);
}

void QtTopCryptosPage::onBackClicked() {
    emit backRequested();
}

void QtTopCryptosPage::onAutoRefreshTimer() {
    fetchTopCryptos();
}

void QtTopCryptosPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateScrollAreaWidth();
}

void QtTopCryptosPage::updateScrollAreaWidth() {
    if (m_scrollArea) {
        int windowWidth = this->width();

        // Apply max-width constraints based on screen size
        if (windowWidth < 768) {
            // Mobile - use full width
            m_scrollArea->setMaximumWidth(QWIDGETSIZE_MAX);
            m_scrollArea->setMinimumWidth(windowWidth);
        } else if (windowWidth > 1920) {
            // Large screens - limit to 65% of width
            int targetWidth = static_cast<int>(windowWidth * 0.65);
            m_scrollArea->setMaximumWidth(targetWidth);
            m_scrollArea->setMinimumWidth(800);
        } else if (windowWidth > 1200) {
            // Medium screens - limit to 75% of width
            int targetWidth = static_cast<int>(windowWidth * 0.75);
            m_scrollArea->setMaximumWidth(targetWidth);
            m_scrollArea->setMinimumWidth(600);
        } else {
            // Small screens - use 100% of width
            m_scrollArea->setMaximumWidth(QWIDGETSIZE_MAX);
            m_scrollArea->setMinimumWidth(windowWidth);
        }
    }
}
