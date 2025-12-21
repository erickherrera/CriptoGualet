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
#include <QScrollBar>
#include <algorithm>

// ============================================================================
// QtCryptoCard Implementation
// ============================================================================

QtCryptoCard::QtCryptoCard(QWidget *parent)
    : QFrame(parent)
    , m_themeManager(&QtThemeManager::instance())
    , m_networkManager(new QNetworkAccessManager(this))
    , m_iconLoaded(false) {
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
    m_iconLabel->setScaledContents(true); // Scale to fit label

    // Crypto info (middle)
    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(4);

    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(12);

    m_symbolLabel = new QLabel(this);
    m_symbolLabel->setStyleSheet("font-size: 20px; font-weight: bold;");

    m_nameLabel = new QLabel(this);
    m_nameLabel->setStyleSheet(QString("font-size: 14px; color: %1;")
        .arg(m_themeManager->dimmedTextColor().name()));

    topRow->addWidget(m_symbolLabel);
    topRow->addWidget(m_nameLabel);
    topRow->addStretch();

    m_marketCapLabel = new QLabel(this);
    m_marketCapLabel->setStyleSheet(QString("font-size: 12px; color: %1;")
        .arg(m_themeManager->dimmedTextColor().name()));

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

    // Enable hover cursor
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
}

void QtCryptoCard::setCryptoData(const PriceService::CryptoPriceData &cryptoData, int rank) {
    QString symbol = QString::fromStdString(cryptoData.symbol);
    m_currentSymbol = symbol;
    m_currentImageUrl = QString::fromStdString(cryptoData.image_url);

    // Set placeholder while loading
    m_iconLabel->clear();
    QColor placeholderBg = m_themeManager->secondaryColor();
    placeholderBg.setAlphaF(0.1f);  // 10% opacity
    m_iconLabel->setStyleSheet(QString("border-radius: 24px; background-color: %1;")
        .arg(placeholderBg.name(QColor::HexArgb)));
    m_iconLoaded = false;

    // Add rank prefix to symbol
    m_symbolLabel->setText(QString("#%1  %2")
        .arg(rank)
        .arg(symbol));
    m_nameLabel->setText(QString::fromStdString(cryptoData.name));
    m_priceLabel->setText(formatPrice(cryptoData.usd_price));
    m_marketCapLabel->setText("MCap: " + formatMarketCap(cryptoData.market_cap));

    // Format price change with color
    QString changeText = QString("%1%2")
        .arg(cryptoData.price_change_24h >= 0 ? "+" : "")
        .arg(cryptoData.price_change_24h, 0, 'f', 2) + "%";
    m_changeLabel->setText(changeText);

    // Apply color based on change
    if (cryptoData.price_change_24h >= 0) {
        m_changeLabel->setStyleSheet(QString(
            "font-size: 14px;"
            "font-weight: 600;"
            "padding: 4px 12px;"
            "border-radius: 12px;"
            "background-color: %1;"
            "color: %2;"
        ).arg(m_themeManager->lightPositive().name(QColor::HexArgb))
         .arg(m_themeManager->positiveColor().name()));
    } else {
        m_changeLabel->setStyleSheet(QString(
            "font-size: 14px;"
            "font-weight: 600;"
            "padding: 4px 12px;"
            "border-radius: 12px;"
            "background-color: %1;"
            "color: %2;"
        ).arg(m_themeManager->lightNegative().name(QColor::HexArgb))
         .arg(m_themeManager->negativeColor().name()));
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

void QtCryptoCard::loadIcon(const QString &symbol) {
    if (m_iconLoaded) {
        return; // Already loaded
    }

    // Use the image URL from the API data, or fallback to static mapping
    QString iconUrl;
    if (!m_currentImageUrl.isEmpty()) {
        iconUrl = m_currentImageUrl;
        qDebug() << "Using API image URL for" << symbol << ":" << iconUrl;
    } else {
        iconUrl = getCryptoIconUrl(symbol);
        qDebug() << "Using fallback static mapping for" << symbol << ":" << iconUrl;
    }

    // Trigger icon download with proper headers
    QNetworkRequest request(iconUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "CriptoGualet/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_networkManager->get(request);
}

QString QtCryptoCard::getCryptoIconUrl(const QString &symbol) {
    // Use CoinGecko assets API
    // Format: https://assets.coingecko.com/coins/images/{id}/large/{coin}.png
    // Expanded to cover top 100+ cryptocurrencies
    static const QMap<QString, QString> symbolToImagePath = {
        // Top 10
        {"BTC", "1/large/bitcoin.png"},
        {"ETH", "279/large/ethereum.png"},
        {"USDT", "325/large/tether.png"},
        {"BNB", "825/large/binance-coin-logo.png"},
        {"SOL", "4128/large/solana.png"},
        {"USDC", "6319/large/usd-coin.png"},
        {"XRP", "44/large/xrp.png"},
        {"STETH", "13442/large/steth_logo.png"},
        {"DOGE", "5/large/dogecoin.png"},
        {"ADA", "975/large/cardano.png"},

        // Top 11-20
        {"TRX", "1094/large/tron-logo.png"},
        {"AVAX", "12559/large/Avalanche_Circle_RedWhite_Trans.png"},
        {"TON", "17980/large/ton_symbol.png"},
        {"WBTC", "7598/large/wrapped_bitcoin_wbtc.png"},
        {"LINK", "877/large/chainlink-new-logo.png"},
        {"SHIB", "11939/large/shiba.png"},
        {"DOT", "12171/large/polkadot.png"},
        {"MATIC", "4713/large/matic-token-icon.png"},
        {"BCH", "780/large/bitcoin-cash-circle.png"},
        {"DAI", "9956/large/Badge_Dai.png"},

        // Top 21-40
        {"LTC", "2/large/litecoin.png"},
        {"UNI", "12504/large/uni.png"},
        {"ATOM", "5/large/cosmos_hub.png"},
        {"ICP", "14541/large/icp_logo.png"},
        {"LEO", "5635/large/unus-sed-leo-leo.png"},
        {"ETC", "1321/large/ethereum-classic-logo.png"},
        {"XLM", "100/large/stellar_lumens.png"},
        {"FIL", "12817/large/filecoin.png"},
        {"XMR", "69/large/monero_logo.png"},
        {"APT", "26455/large/aptos-1.png"},
        {"OKB", "3897/large/okb-coin.png"},
        {"HBAR", "4642/large/hedera-hashgraph-logo.png"},
        {"MNT", "27075/large/mantle.png"},
        {"NEAR", "16165/large/near.png"},
        {"CRO", "7310/large/cro.png"},
        {"RNDR", "5690/large/RNDR-token.png"},
        {"KAS", "25751/large/kaspa.png"},
        {"IMX", "17233/large/immutablex.png"},
        {"ARB", "11841/large/arbitrum.png"},
        {"OP", "11840/large/optimism.png"},

        // Top 41-60
        {"VET", "1817/large/vethor-token.png"},
        {"STX", "4847/large/stacks-logo.png"},
        {"GRT", "13139/large/GRT.png"},
        {"MKR", "1364/large/maker.png"},
        {"INJ", "12882/large/Injective_Protocol.png"},
        {"ALGO", "9/large/algorand.png"},
        {"RUNE", "4/large/rune.png"},
        {"QNT", "4/large/quant.png"},
        {"AAVE", "12645/large/AAVE.png"},
        {"FLR", "25/large/flare-network-logo.png"},
        {"SNX", "5/large/synthetix_snx_logo.png"},
        {"EGLD", "12335/large/EGLD_token.png"},
        {"FTM", "16746/large/fantom_logo.png"},
        {"XTZ", "976/large/tezos-logo.png"},
        {"SAND", "12220/large/sand_logo.jpg"},
        {"THETA", "2416/large/theta-token-logo.png"},
        {"MANA", "2/large/decentraland-mana.png"},
        {"EOS", "1765/large/eos.png"},
        {"XDC", "9/large/xinfin-network.png"},
        {"AXS", "17/large/axie-infinity.png"},

        // Top 61-80
        {"FLOW", "4558/large/flow.png"},
        {"NEO", "480/large/neo-logo.png"},
        {"KLAY", "9672/large/klaytn.png"},
        {"CHZ", "8834/large/chiliz.png"},
        {"USDD", "24/large/USDD_Token.png"},
        {"TUSD", "3449/large/true-usd.png"},
        {"PEPE", "29850/large/pepe-token.png"},
        {"CFX", "7334/large/conflux-logo.png"},
        {"ZEC", "486/large/zec.png"},
        {"MIOTA", "1720/large/iota_logo.png"},
        {"LDO", "17949/large/lido-dao.png"},
        {"BSV", "3602/large/bitcoin-sv.png"},
        {"KAVA", "4846/large/kava.png"},
        {"DASH", "131/large/dash-logo.png"},
        {"HT", "2502/large/huobi_token_logo.png"},
        {"1INCH", "13718/large/1inch-logo.png"},
        {"CAKE", "12632/large/cake.png"},
        {"GMX", "18323/large/gmx-coin.png"},
        {"RPL", "2/large/rocket-pool.png"},
        {"ZIL", "1520/large/zilliqa.png"},

        // Top 81-100
        {"ENJ", "1102/large/enjin-coin-logo.png"},
        {"BAT", "3/large/basic-attention-token-logo.png"},
        {"COMP", "10775/large/Compound.png"},
        {"YFI", "11849/large/yearn.png"},
        {"SUI", "26375/large/sui-logo.png"},
        {"BLUR", "28453/large/blur-icon.png"},
        {"CRV", "12124/large/Curve.png"},
        {"GALA", "12493/large/gala.png"},
        {"CHSB", "2499/large/swissborg_logo.png"},
        {"FXS", "13222/large/frax-share.png"},
        {"LRC", "913/large/loopring.png"},
        {"ZRX", "1896/large/0x_protocol.png"},
        {"SUSHI", "12271/large/sushiswap.png"},
        {"ONE", "11696/large/harmony-one-logo.png"},
        {"WAVES", "1274/large/waves-logo.png"},
        {"CELO", "11645/large/celo-logo.png"},
        {"ICX", "2099/large/icon.png"},
        {"WOO", "7501/large/wootrade.png"},
        {"QTUM", "1684/large/qtum.png"},
        {"AR", "5186/large/arweave.png"}
    };

    QString imagePath = symbolToImagePath.value(symbol.toUpper(), "1/large/bitcoin.png");
    return QString("https://assets.coingecko.com/coins/images/%1").arg(imagePath);
}

void QtCryptoCard::onIconDownloaded(QNetworkReply *reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray imageData = reply->readAll();
        QPixmap pixmap;

        if (pixmap.loadFromData(imageData)) {
            // Use higher resolution for better quality (2x the display size)
            int highResSize = 96;

            // Scale pixmap to high resolution with smooth transformation
            QPixmap scaledPixmap = pixmap.scaled(
                highResSize, highResSize,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );

            // Create a rounded pixmap at high resolution
            QPixmap roundedPixmap(highResSize, highResSize);
            roundedPixmap.fill(Qt::transparent);

            QPainter painter(&roundedPixmap);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setRenderHint(QPainter::SmoothPixmapTransform);

            QPainterPath path;
            path.addEllipse(0, 0, highResSize, highResSize);
            painter.setClipPath(path);

            // Center the scaled pixmap
            int x = (highResSize - scaledPixmap.width()) / 2;
            int y = (highResSize - scaledPixmap.height()) / 2;
            painter.drawPixmap(x, y, scaledPixmap);

            // Label will scale this down to 48x48 with smooth transformation
            m_iconLabel->setPixmap(roundedPixmap);
            m_iconLabel->setStyleSheet("");
            m_iconLoaded = true;

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
    // Use higher resolution for better quality
    int highResSize = 96;

    // Create a simple colored circle as fallback at high resolution
    QPixmap fallback(highResSize, highResSize);
    fallback.fill(Qt::transparent);

    QPainter painter(&fallback);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw colored circle
    painter.setBrush(QColor(100, 116, 139, 50));
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(0, 0, highResSize, highResSize);

    // Draw currency symbol
    painter.setPen(QColor(100, 116, 139));
    QFont font = painter.font();
    font.setPointSize(36); // Scaled up for high-res
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRect(0, 0, highResSize, highResSize), Qt::AlignCenter, "$");

    // Label will scale this down to 48x48 with smooth transformation
    m_iconLabel->setPixmap(fallback);
    m_iconLabel->setStyleSheet("");
}

void QtCryptoCard::applyTheme() {
    QString cardBg = m_themeManager->surfaceColor().name();
    QColor surfaceColor = m_themeManager->surfaceColor();

    // Create lighter color for hover state
    QColor hoverColor = surfaceColor.lighter(110);

    QString cardStyle = QString(
        "QFrame {"
        "  background-color: %1;"
        "  border-radius: 16px;"
        "  border: none;"
        "}"
        "QFrame:hover {"
        "  background-color: %2;"
        "}"
    ).arg(cardBg, hoverColor.name());

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
    , m_currentSortIndex(0)
    , m_searchDebounceTimer(new QTimer(this))
    , m_refreshTimer(new QTimer(this)) {

    setupUI();
    applyTheme();

    // Connect to theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &QtTopCryptosPage::applyTheme);

    // Connect refresh timer (auto-refresh every 60 seconds)
    connect(m_refreshTimer, &QTimer::timeout, this, &QtTopCryptosPage::onAutoRefreshTimer);
    m_refreshTimer->start(60000);

    // Configure search debounce timer
    m_searchDebounceTimer->setSingleShot(true);
    connect(m_searchDebounceTimer, &QTimer::timeout, this, &QtTopCryptosPage::onSearchDebounceTimer);

    // Initial data fetch
    fetchTopCryptos();
}

void QtTopCryptosPage::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(
        m_themeManager->standardMargin(),
        m_themeManager->standardMargin(),
        m_themeManager->standardMargin(),
        m_themeManager->standardMargin()
    );
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
    m_contentLayout->setContentsMargins(
        m_themeManager->spacing(8),      // 32px
        m_themeManager->standardSpacing(),  // 16px (was 24)
        m_themeManager->spacing(8),      // 32px
        m_themeManager->standardSpacing()   // 16px (was 24)
    );
    m_contentLayout->setSpacing(m_themeManager->standardSpacing());

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

    // Connect scroll area scrolling to icon loading
    connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() {
                QTimer::singleShot(50, this, &QtTopCryptosPage::loadVisibleIcons);
            });

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

    m_titleLabel = new QLabel("Top 100 Cryptocurrencies", m_headerWidget);
    m_titleLabel->setProperty("class", "title");

    m_subtitleLabel = new QLabel("Live prices updated every 60 seconds", m_headerWidget);
    m_subtitleLabel->setProperty("class", "subtitle");

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_subtitleLabel);

    headerLayout->addLayout(topRow);
    headerLayout->addLayout(titleLayout);

    // Search and filter controls row
    QHBoxLayout *controlsRow = new QHBoxLayout();
    controlsRow->setSpacing(12);

    createSearchBar();
    createSortDropdown();

    controlsRow->addWidget(m_searchBox, 1);
    controlsRow->addWidget(m_sortDropdown);

    headerLayout->addLayout(controlsRow);

    // Result counter row
    QHBoxLayout *counterRow = new QHBoxLayout();
    m_resultCounterLabel = new QLabel("Loading...", m_headerWidget);
    m_resultCounterLabel->setStyleSheet(QString("font-size: 13px; font-weight: 500; color: %1;")
        .arg(m_themeManager->dimmedTextColor().name()));
    counterRow->addWidget(m_resultCounterLabel);
    counterRow->addStretch();

    headerLayout->addLayout(counterRow);
}

void QtTopCryptosPage::createSearchBar() {
    m_searchBox = new QLineEdit(m_headerWidget);
    m_searchBox->setPlaceholderText("Search by name or symbol (e.g., Bitcoin, BTC)...");
    m_searchBox->setFixedHeight(44);
    m_searchBox->setClearButtonEnabled(false); // We'll use custom clear button

    // Connect search text changes with debouncing
    connect(m_searchBox, &QLineEdit::textChanged, this, &QtTopCryptosPage::onSearchTextChanged);
}

void QtTopCryptosPage::createSortDropdown() {
    m_sortDropdown = new QComboBox(m_headerWidget);
    m_sortDropdown->setFixedHeight(44);
    m_sortDropdown->setCursor(Qt::PointingHandCursor);

    // Add sort options
    m_sortDropdown->addItem("Rank (Default)");
    m_sortDropdown->addItem("Price: High to Low");
    m_sortDropdown->addItem("Price: Low to High");
    m_sortDropdown->addItem("24h Change: Highest Gainers");
    m_sortDropdown->addItem("24h Change: Biggest Losers");
    m_sortDropdown->addItem("Market Cap: Largest");
    m_sortDropdown->addItem("Market Cap: Smallest");
    m_sortDropdown->addItem("Name: A-Z");
    m_sortDropdown->addItem("Name: Z-A");

    // Connect sort changes
    connect(m_sortDropdown, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &QtTopCryptosPage::onSortChanged);
}

void QtTopCryptosPage::createCryptoCards() {
    // Clear existing cards
    for (auto *card : m_cryptoCards) {
        card->deleteLater();
    }
    m_cryptoCards.clear();

    qDebug() << "Creating 100 crypto card widgets...";

    // Create 100 cards for top 100 cryptocurrencies
    for (int i = 0; i < 100; ++i) {
        QtCryptoCard *card = new QtCryptoCard(m_cardsContainer);
        card->setMinimumHeight(100);
        card->setVisible(false); // Hide until we have data
        m_cryptoCards.append(card);
        m_cardsLayout->addWidget(card);
    }

    qDebug() << "Created 100 crypto cards";
}

void QtTopCryptosPage::fetchTopCryptos() {
    qDebug() << "Fetching top 100 cryptocurrencies...";

    // Show loading state
    m_subtitleLabel->setText("Loading cryptocurrency data...");
    m_resultCounterLabel->setText("Loading...");

    // Disable controls during fetch
    m_searchBox->setEnabled(false);
    m_sortDropdown->setEnabled(false);
    m_refreshButton->setEnabled(false);

    // Fetch data (this is a blocking call)
    m_cryptoData = m_priceFetcher->GetTopCryptosByMarketCap(100);

    qDebug() << "Fetch complete. Retrieved" << m_cryptoData.size() << "items";

    // Re-enable controls
    m_searchBox->setEnabled(true);
    m_sortDropdown->setEnabled(true);
    m_refreshButton->setEnabled(true);

    // Apply filters and sorting
    filterAndSortData();
}

void QtTopCryptosPage::updateCards() {
    if (m_filteredData.empty() && !m_cryptoData.empty()) {
        // Show empty state - no search results
        qDebug() << "No results match the current search criteria";

        // Update subtitle to show empty state
        m_subtitleLabel->setText("No cryptocurrencies match your search. Try different keywords.");
        QString subtitleColor = m_themeManager->subtitleColor().name();
        QFont subtitleFont = m_themeManager->textFont();
        subtitleFont.setPointSize(12);
        m_subtitleLabel->setFont(subtitleFont);
        m_subtitleLabel->setStyleSheet(QString("color: %1;").arg(subtitleColor));

        // Hide all cards
        for (auto *card : m_cryptoCards) {
            card->setVisible(false);
        }
        return;
    }

    if (m_cryptoData.empty()) {
        // Show error state - API failed
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

    qDebug() << "Displaying" << m_filteredData.size() << "cryptocurrencies";

    // Reset subtitle
    m_subtitleLabel->setText("Live prices updated every 60 seconds");
    QString subtitleColor = m_themeManager->subtitleColor().name();
    QFont subtitleFont = m_themeManager->textFont();
    subtitleFont.setPointSize(12);
    m_subtitleLabel->setFont(subtitleFont);
    m_subtitleLabel->setStyleSheet(QString("color: %1;").arg(subtitleColor));

    // Update result counter
    updateResultCounter();

    // Display filtered data
    int rank = 1;
    for (int i = 0; i < static_cast<int>(m_filteredData.size()) && i < m_cryptoCards.size(); ++i) {
        qDebug() << "Setting card" << i << ":" << QString::fromStdString(m_filteredData[i].name);
        m_cryptoCards[i]->setCryptoData(m_filteredData[i], rank++);
        m_cryptoCards[i]->setVisible(true);
    }

    // Hide unused cards
    for (int i = static_cast<int>(m_filteredData.size()); i < m_cryptoCards.size(); ++i) {
        m_cryptoCards[i]->setVisible(false);
    }

    // Load icons for visible cards with a slight delay for better UX
    QTimer::singleShot(100, this, &QtTopCryptosPage::loadVisibleIcons);
}

void QtTopCryptosPage::filterAndSortData() {
    // Start with all data
    m_filteredData = m_cryptoData;

    // Apply search filter
    applySearchFilter();

    // Apply sorting
    applySorting();

    // Update display
    updateCards();
}

void QtTopCryptosPage::applySearchFilter() {
    if (m_searchText.isEmpty()) {
        // No filter, use all data
        m_filteredData = m_cryptoData;
        return;
    }

    QString searchLower = m_searchText.toLower();
    m_filteredData.clear();

    for (const auto& crypto : m_cryptoData) {
        QString name = QString::fromStdString(crypto.name).toLower();
        QString symbol = QString::fromStdString(crypto.symbol).toLower();

        if (name.contains(searchLower) || symbol.contains(searchLower)) {
            m_filteredData.push_back(crypto);
        }
    }

    qDebug() << "Filtered from" << m_cryptoData.size() << "to" << m_filteredData.size() << "results";
}

void QtTopCryptosPage::applySorting() {
    switch (m_currentSortIndex) {
        case 0: // Rank (Default) - already sorted by market cap from API
            break;

        case 1: // Price: High to Low
            std::sort(m_filteredData.begin(), m_filteredData.end(),
                [](const PriceService::CryptoPriceData& a, const PriceService::CryptoPriceData& b) {
                    return a.usd_price > b.usd_price;
                });
            break;

        case 2: // Price: Low to High
            std::sort(m_filteredData.begin(), m_filteredData.end(),
                [](const PriceService::CryptoPriceData& a, const PriceService::CryptoPriceData& b) {
                    return a.usd_price < b.usd_price;
                });
            break;

        case 3: // 24h Change: Highest Gainers
            std::sort(m_filteredData.begin(), m_filteredData.end(),
                [](const PriceService::CryptoPriceData& a, const PriceService::CryptoPriceData& b) {
                    return a.price_change_24h > b.price_change_24h;
                });
            break;

        case 4: // 24h Change: Biggest Losers
            std::sort(m_filteredData.begin(), m_filteredData.end(),
                [](const PriceService::CryptoPriceData& a, const PriceService::CryptoPriceData& b) {
                    return a.price_change_24h < b.price_change_24h;
                });
            break;

        case 5: // Market Cap: Largest
            std::sort(m_filteredData.begin(), m_filteredData.end(),
                [](const PriceService::CryptoPriceData& a, const PriceService::CryptoPriceData& b) {
                    return a.market_cap > b.market_cap;
                });
            break;

        case 6: // Market Cap: Smallest
            std::sort(m_filteredData.begin(), m_filteredData.end(),
                [](const PriceService::CryptoPriceData& a, const PriceService::CryptoPriceData& b) {
                    return a.market_cap < b.market_cap;
                });
            break;

        case 7: // Name: A-Z
            std::sort(m_filteredData.begin(), m_filteredData.end(),
                [](const PriceService::CryptoPriceData& a, const PriceService::CryptoPriceData& b) {
                    return a.name < b.name;
                });
            break;

        case 8: // Name: Z-A
            std::sort(m_filteredData.begin(), m_filteredData.end(),
                [](const PriceService::CryptoPriceData& a, const PriceService::CryptoPriceData& b) {
                    return a.name > b.name;
                });
            break;
    }

    qDebug() << "Applied sorting option" << m_currentSortIndex;
}

void QtTopCryptosPage::updateResultCounter() {
    if (m_searchText.isEmpty()) {
        m_resultCounterLabel->setText(QString("Showing all %1 cryptocurrencies")
            .arg(m_filteredData.size()));
    } else {
        m_resultCounterLabel->setText(QString("Showing %1 of %2 cryptocurrencies")
            .arg(m_filteredData.size())
            .arg(m_cryptoData.size()));
    }
}

void QtTopCryptosPage::loadVisibleIcons() {
    if (!m_scrollArea) return;

    // Get the visible rectangle of the scroll area
    QRect visibleRect = m_scrollArea->viewport()->rect();
    int scrollOffset = m_scrollArea->verticalScrollBar()->value();

    // Load icons for visible cards plus a buffer
    for (int i = 0; i < static_cast<int>(m_filteredData.size()) && i < m_cryptoCards.size(); ++i) {
        QtCryptoCard* card = m_cryptoCards[i];
        if (!card->isVisible()) continue;

        // Check if card is in or near the viewport
        QRect cardGeometry = card->geometry();
        cardGeometry.moveTop(cardGeometry.top() - scrollOffset);

        // Load if in viewport or within 500px buffer
        if (cardGeometry.bottom() >= -500 && cardGeometry.top() <= visibleRect.height() + 500) {
            if (!card->isIconLoaded()) {
                QString symbol = QString::fromStdString(m_filteredData[i].symbol);
                card->loadIcon(symbol);
            }
        }
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

    // Search box styling
    QString searchBoxStyle = QString(
        "QLineEdit {"
        "  background-color: %1;"
        "  border: 2px solid rgba(%2, %3, %4, 0.3);"
        "  border-radius: 8px;"
        "  color: %5;"
        "  font-size: 14px;"
        "  padding: 10px 16px;"
        "}"
        "QLineEdit:focus {"
        "  border: 2px solid %6;"
        "}"
        "QLineEdit:disabled {"
        "  opacity: 0.5;"
        "}"
    ).arg(surfaceColor)
     .arg(m_themeManager->accentColor().red())
     .arg(m_themeManager->accentColor().green())
     .arg(m_themeManager->accentColor().blue())
     .arg(txtColor)
     .arg(accentColor);
    m_searchBox->setStyleSheet(searchBoxStyle);

    // Sort dropdown styling
    QString dropdownStyle = QString(
        "QComboBox {"
        "  background-color: %1;"
        "  border: 2px solid rgba(%2, %3, %4, 0.3);"
        "  border-radius: 8px;"
        "  color: %5;"
        "  font-size: 14px;"
        "  padding: 10px 16px;"
        "  min-width: 200px;"
        "}"
        "QComboBox:hover {"
        "  border: 2px solid rgba(%2, %3, %4, 0.5);"
        "}"
        "QComboBox:disabled {"
        "  opacity: 0.5;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  padding-right: 10px;"
        "}"
        "QComboBox::down-arrow {"
        "  width: 12px;"
        "  height: 12px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: %1;"
        "  border: 2px solid rgba(%2, %3, %4, 0.3);"
        "  border-radius: 8px;"
        "  color: %5;"
        "  selection-background-color: %6;"
        "  padding: 4px;"
        "}"
    ).arg(surfaceColor)
     .arg(m_themeManager->accentColor().red())
     .arg(m_themeManager->accentColor().green())
     .arg(m_themeManager->accentColor().blue())
     .arg(txtColor)
     .arg(accentColor);
    m_sortDropdown->setStyleSheet(dropdownStyle);

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

void QtTopCryptosPage::onSearchTextChanged(const QString &text) {
    m_searchText = text;

    // Stop existing timer
    m_searchDebounceTimer->stop();

    // Start new timer (300ms debounce)
    m_searchDebounceTimer->start(300);
}

void QtTopCryptosPage::onSortChanged(int index) {
    m_currentSortIndex = index;
    qDebug() << "Sort changed to index" << index;

    // Re-apply filter and sort
    filterAndSortData();
}

void QtTopCryptosPage::onSearchDebounceTimer() {
    qDebug() << "Search debounce triggered, searching for:" << m_searchText;

    // Apply filter and sort
    filterAndSortData();
}

void QtTopCryptosPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateScrollAreaWidth();

    // Reload icons when viewport changes
    QTimer::singleShot(100, this, &QtTopCryptosPage::loadVisibleIcons);
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
