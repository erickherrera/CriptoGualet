#include "QtExpandableWalletCard.h"
#include "QtThemeManager.h"
#include <QEvent>
#include <QMouseEvent>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QDebug>

QtExpandableWalletCard::QtExpandableWalletCard(QtThemeManager *themeManager,
                                               QWidget *parent)
    : QFrame(parent), m_themeManager(themeManager), m_isExpanded(false),
      m_networkManager(new QNetworkAccessManager(this)) {
  setupUI();
  applyTheme();

  connect(m_networkManager, &QNetworkAccessManager::finished,
          this, &QtExpandableWalletCard::onIconDownloaded);
}

void QtExpandableWalletCard::setupUI() {
  setProperty("class", "card");
  setObjectName("expandableWalletCard");

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(25, 25, 25, 25);
  mainLayout->setSpacing(0);

  // Collapsed header (clickable)
  m_collapsedHeader = new QWidget(this);
  m_collapsedHeader->setObjectName("walletButton");
  m_collapsedHeader->setCursor(Qt::PointingHandCursor);
  m_collapsedHeader->installEventFilter(this);

  QHBoxLayout *collapsedLayout = new QHBoxLayout(m_collapsedHeader);
  collapsedLayout->setContentsMargins(20, 12, 20, 12);
  collapsedLayout->setSpacing(15);

  // Crypto logo container with background (circular)
  QWidget *logoContainer = new QWidget(m_collapsedHeader);
  logoContainer->setObjectName("logoContainer");
  logoContainer->setFixedSize(48, 48); // Increased from 40x40 for better clarity

  QHBoxLayout *logoLayout = new QHBoxLayout(logoContainer);
  logoLayout->setContentsMargins(0, 0, 0, 0);
  logoLayout->setAlignment(Qt::AlignCenter);

  m_cryptoLogo = new QLabel("₿", logoContainer);
  m_cryptoLogo->setObjectName("cryptoLogo");
  m_cryptoLogo->setAlignment(Qt::AlignCenter);
  m_cryptoLogo->setScaledContents(false); // Preserve aspect ratio
  logoLayout->addWidget(m_cryptoLogo);

  collapsedLayout->addWidget(logoContainer);

  // Balance container
  QWidget *balanceContainer = new QWidget(m_collapsedHeader);
  balanceContainer->setObjectName("balanceContainer");
  QVBoxLayout *balanceLayout = new QVBoxLayout(balanceContainer);
  balanceLayout->setContentsMargins(0, 0, 0, 0);
  balanceLayout->setSpacing(2);

  m_cryptoName = new QLabel("BITCOIN", balanceContainer);
  m_cryptoName->setObjectName("cryptoName");
  balanceLayout->addWidget(m_cryptoName);

  m_balanceLabel = new QLabel("0.00000000 BTC", balanceContainer);
  m_balanceLabel->setObjectName("balanceAmount");
  balanceLayout->addWidget(m_balanceLabel);

  collapsedLayout->addWidget(balanceContainer);
  collapsedLayout->addStretch();

  // Expand indicator
  m_expandIndicator = new QLabel("⌄", m_collapsedHeader);
  m_expandIndicator->setObjectName("expandIndicator");
  collapsedLayout->addWidget(m_expandIndicator);

  mainLayout->addWidget(m_collapsedHeader);

  // Expanded content
  m_expandedContent = new QWidget(this);
  m_expandedContent->setObjectName("expandedCard");
  m_expandedContent->setVisible(false);

  QVBoxLayout *expandedLayout = new QVBoxLayout(m_expandedContent);
  expandedLayout->setContentsMargins(25, 20, 25, 25);
  expandedLayout->setSpacing(20);

  // Action buttons
  QHBoxLayout *actionsLayout = new QHBoxLayout();
  actionsLayout->setSpacing(15);

  m_sendButton = new QPushButton("Send", m_expandedContent);
  m_sendButton->setObjectName("actionButton");
  m_sendButton->setCursor(Qt::PointingHandCursor);
  connect(m_sendButton, &QPushButton::clicked, this,
          &QtExpandableWalletCard::sendRequested);

  m_receiveButton = new QPushButton("Receive", m_expandedContent);
  m_receiveButton->setObjectName("actionButton");
  m_receiveButton->setCursor(Qt::PointingHandCursor);
  connect(m_receiveButton, &QPushButton::clicked, this,
          &QtExpandableWalletCard::receiveRequested);

  actionsLayout->addWidget(m_sendButton);
  actionsLayout->addWidget(m_receiveButton);
  expandedLayout->addLayout(actionsLayout);

  // Transaction history section
  QWidget *historySection = new QWidget(m_expandedContent);
  historySection->setObjectName("historySection");
  QVBoxLayout *historyLayout = new QVBoxLayout(historySection);
  historyLayout->setContentsMargins(15, 15, 15, 15);
  historyLayout->setSpacing(10);

  m_historyTitleLabel = new QLabel("Transaction History", historySection);
  m_historyTitleLabel->setObjectName("historyTitle");
  historyLayout->addWidget(m_historyTitleLabel);

  m_historyText = new QTextEdit(historySection);
  m_historyText->setReadOnly(true);
  m_historyText->setObjectName("historyText");
  m_historyText->setMinimumHeight(150);
  historyLayout->addWidget(m_historyText);

  expandedLayout->addWidget(historySection);

  mainLayout->addWidget(m_expandedContent);
}

void QtExpandableWalletCard::setCryptocurrency(const QString &name,
                                               const QString &symbol,
                                               const QString &logoText) {
  m_cryptoName->setText(name.toUpper());
  m_cryptoSymbol = symbol;

  // Download crypto icon
  QString iconUrl = getCryptoIconUrl(symbol);

  // Set placeholder while loading
  m_cryptoLogo->clear();
  m_cryptoLogo->setStyleSheet("border-radius: 24px; background-color: rgba(100, 116, 139, 0.1);");

  // Trigger icon download with proper headers
  QNetworkRequest request(iconUrl);
  request.setHeader(QNetworkRequest::UserAgentHeader, "CriptoGualet/1.0");
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  m_networkManager->get(request);
}

void QtExpandableWalletCard::setBalance(const QString &balance) {
  m_balanceLabel->setText(balance);
}

void QtExpandableWalletCard::setTransactionHistory(const QString &historyHtml) {
  m_historyText->setHtml(historyHtml);
}

QString QtExpandableWalletCard::getCryptoIconUrl(const QString &symbol) {
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

void QtExpandableWalletCard::onIconDownloaded(QNetworkReply *reply) {
  if (reply->error() == QNetworkReply::NoError) {
    QByteArray imageData = reply->readAll();
    QPixmap pixmap;

    if (pixmap.loadFromData(imageData)) {
      // Get device pixel ratio for high DPI support
      qreal dpr = devicePixelRatioF();
      int iconSize = 48; // Increased from 40 for better clarity

      // Scale pixmap with high DPI support
      QPixmap scaledPixmap = pixmap.scaled(
          qRound(iconSize * dpr), qRound(iconSize * dpr),
          Qt::KeepAspectRatio,
          Qt::SmoothTransformation
      );
      scaledPixmap.setDevicePixelRatio(dpr);

      m_cryptoLogo->setPixmap(scaledPixmap);
      m_cryptoLogo->setStyleSheet("background: transparent; border: none;");

      qDebug() << "Successfully loaded wallet icon from" << reply->url().toString();
    } else {
      qDebug() << "Failed to load image data from" << reply->url().toString();
      setFallbackIcon();
    }
  } else {
    qDebug() << "Network error downloading wallet icon:" << reply->errorString()
             << "from" << reply->url().toString();
    setFallbackIcon();
  }
  reply->deleteLater();
}

void QtExpandableWalletCard::setFallbackIcon() {
  // Get device pixel ratio for high DPI support
  qreal dpr = devicePixelRatioF();
  int iconSize = 48;
  int scaledSize = qRound(iconSize * dpr);

  // Create a simple colored circle as fallback with Bitcoin symbol
  QPixmap fallback(scaledSize, scaledSize);
  fallback.fill(Qt::transparent);
  fallback.setDevicePixelRatio(dpr);

  QPainter painter(&fallback);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw colored circle
  painter.setBrush(QColor(100, 116, 139, 50));
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(0, 0, scaledSize, scaledSize);

  // Draw Bitcoin symbol
  painter.setPen(QColor(100, 116, 139));
  QFont font = painter.font();
  font.setPointSize(20);
  font.setBold(true);
  painter.setFont(font);
  painter.drawText(QRect(0, 0, scaledSize, scaledSize), Qt::AlignCenter, "₿");

  m_cryptoLogo->setPixmap(fallback);
  m_cryptoLogo->setStyleSheet("");
}

void QtExpandableWalletCard::toggleExpanded() {
  m_isExpanded = !m_isExpanded;
  m_expandedContent->setVisible(m_isExpanded);
  m_expandIndicator->setText(m_isExpanded ? "⌃" : "⌄");
}

bool QtExpandableWalletCard::eventFilter(QObject *obj, QEvent *event) {
  if (obj == m_collapsedHeader && event->type() == QEvent::MouseButtonPress) {
    toggleExpanded();
    return true;
  }
  return QFrame::eventFilter(obj, event);
}

void QtExpandableWalletCard::applyTheme() {
  updateStyles();
}

void QtExpandableWalletCard::updateStyles() {
  if (!m_themeManager)
    return;

  QString surface = m_themeManager->surfaceColor().name();
  QString accent = m_themeManager->accentColor().name();
  QString text = m_themeManager->textColor().name();
  QString textSecondary = m_themeManager->subtitleColor().name();
  QString primary = m_themeManager->primaryColor().name();
  QString background = m_themeManager->backgroundColor().name();

  // Determine if dark theme for better contrast
  bool isDarkTheme = m_themeManager->surfaceColor().lightness() < 128;

  // Card styling with rounded corners
  QString walletCardCss = QString(R"(
    QFrame#expandableWalletCard {
      background-color: %1;
      border: 1px solid %2;
      border-radius: 12px;
    }
    QWidget#walletButton {
      background-color: %2;
      border: none;
      border-top-left-radius: 12px;
      border-top-right-radius: 12px;
    }
    QWidget#walletButton:hover {
      background-color: %3;
    }
    QWidget#expandedCard {
      background-color: %1;
      border-top: 1px solid %2;
      border-bottom-left-radius: 12px;
      border-bottom-right-radius: 12px;
    }
    QWidget#logoContainer {
      background-color: transparent;
      border: none;
      border-radius: 24px;
    }
    QWidget#balanceContainer {
      background-color: transparent;
      border: none;
    }
    QWidget#historySection {
      background-color: %4;
      border: 1px solid %5;
      border-radius: 12px;
    }
  )")
                              .arg(surface)                                             // %1 - card background
                              .arg(accent)                                              // %2 - header/border
                              .arg(m_themeManager->accentColor().lighter(110).name())   // %3 - hover
                              .arg(primary)                                             // %4 - history section bg
                              .arg(m_themeManager->secondaryColor().name());            // %5 - history section border

  setStyleSheet(walletCardCss);

  // Logo styling with high contrast
  // Use accent color for icon on foreground background for extra contrast
  QString logoStyle = QString(R"(
    QLabel#cryptoLogo {
      color: %1;
      font-size: 22px;
      font-weight: bold;
      background: transparent;
      border: none;
    }
  )").arg(accent);
  m_cryptoLogo->setStyleSheet(logoStyle);

  // Crypto name styling - light text on dark themes, dark text on light themes
  QString cryptoNameColor = isDarkTheme
      ? m_themeManager->textColor().lighter(130).name()
      : m_themeManager->textColor().darker(150).name();

  QString nameStyle = QString(R"(
    QLabel#cryptoName {
      color: %1;
      font-size: 10px;
      font-weight: 600;
      letter-spacing: 0.5px;
      background-color: transparent;
    }
  )").arg(cryptoNameColor);
  m_cryptoName->setStyleSheet(nameStyle);

  // Balance styling with better contrast
  // Use brighter text color for better visibility
  QString balanceColor = isDarkTheme ? m_themeManager->textColor().lighter(120).name() : text;
  QString balanceStyle = QString(R"(
    QLabel#balanceAmount {
      color: %1;
      font-size: 14px;
      font-weight: 700;
      background-color: transparent;
    }
  )").arg(balanceColor);
  m_balanceLabel->setStyleSheet(balanceStyle);

  // Expand indicator styling - use accent color for better visibility
  QString indicatorStyle = QString(R"(
    QLabel#expandIndicator {
      color: %1;
      font-size: 18px;
      font-weight: bold;
      background-color: transparent;
    }
  )").arg(accent);
  m_expandIndicator->setStyleSheet(indicatorStyle);

  // Action button styling with rounded corners
  QString buttonStyle = QString(R"(
    QPushButton#actionButton {
      background-color: %1;
      color: %2;
      border: none;
      border-radius: 8px;
      padding: 12px 24px;
      font-size: 14px;
      font-weight: 600;
      text-align: center;
    }
    QPushButton#actionButton:hover {
      background-color: %3;
    }
  )")
                          .arg(accent)
                          .arg(text)
                          .arg(m_themeManager->accentColor().lighter(110).name());
  m_sendButton->setStyleSheet(buttonStyle);
  m_receiveButton->setStyleSheet(buttonStyle);

  // History title styling using theme text color
  QString historyTitleStyle = QString(R"(
    QLabel#historyTitle {
      color: %1;
      font-size: 15px;
      font-weight: 700;
      background-color: transparent;
    }
  )").arg(text);
  m_historyTitleLabel->setStyleSheet(historyTitleStyle);

  // History text area styling - use theme colors for consistency
  QString textEditStyle = QString(R"(
    QTextEdit#historyText {
      background-color: %1;
      color: %2;
      border: 1px solid %3;
      border-radius: 8px;
      padding: 12px;
      font-family: %4;
      font-size: %5px;
    }
  )")
                            .arg(background)                                // background color
                            .arg(text)                                      // text color
                            .arg(m_themeManager->secondaryColor().name())   // border using secondary
                            .arg(m_themeManager->textFont().family())
                            .arg(m_themeManager->textFont().pointSize());
  m_historyText->setStyleSheet(textEditStyle);
}
