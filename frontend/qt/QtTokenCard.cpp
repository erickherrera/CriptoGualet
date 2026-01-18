#include "QtTokenCard.h"
#include "QtThemeManager.h"
#include <QMouseEvent>
#include <QEnterEvent>
#include <QNetworkRequest>
#include <QPixmap>

QtTokenCard::QtTokenCard(QtThemeManager* themeManager, QWidget* parent)
    : QFrame(parent)
    , m_themeManager(themeManager)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_isHovered(false)
{
    setupUI();
    applyTheme();

    connect(m_networkManager, &QNetworkAccessManager::finished, this, &QtTokenCard::onIconDownloaded);
}

void QtTokenCard::setTokenData(const TokenCardData& tokenData) {
    m_tokenData = tokenData;

    m_tokenSymbol->setText(tokenData.symbol);
    m_tokenName->setText(tokenData.name);
    m_tokenBalance->setText(tokenData.balance);
    m_tokenBalanceUSD->setText(tokenData.balanceUSD);

    // Load icon for token using symbol-based URL like the other overload
    QString iconUrl = getTokenIconUrl(tokenData.symbol);
    QNetworkRequest request{QUrl(iconUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "CriptoGualet/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_networkManager->get(request);
}

void QtTokenCard::setTokenData(const QString& contractAddress, const QString& name, const QString& symbol, int decimals) {
    m_tokenData.contractAddress = contractAddress;
    m_tokenData.name = name;
    m_tokenData.symbol = symbol;
    m_tokenData.decimals = decimals;

    m_tokenSymbol->setText(symbol);
    m_tokenName->setText(name);

    QString iconUrl = getTokenIconUrl(symbol);
    QNetworkRequest request{QUrl(iconUrl)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "CriptoGualet/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_networkManager->get(request);
}

void QtTokenCard::setBalance(const QString& balance) {
    m_tokenData.balance = balance;
    m_tokenBalance->setText(balance);
}

void QtTokenCard::setBalanceUSD(const QString& balanceUSD) {
    m_tokenData.balanceUSD = balanceUSD;
    m_tokenBalanceUSD->setText(balanceUSD);
}

void QtTokenCard::applyTheme() {
    if (!m_themeManager) {
        return;
    }

    QString baseStyle = m_themeManager->getMainWindowStyleSheet();
    QString labelStyle = m_themeManager->getLabelStyleSheet();
    QString buttonStyle = m_themeManager->getButtonStyleSheet();

    setStyleSheet(baseStyle);
    m_container->setStyleSheet("QFrame { background-color: " + m_themeManager->surfaceColor().name() + "; border-radius: " + QString::number(m_themeManager->borderRadiusMedium()) + "px; border: 1px solid " + m_themeManager->defaultBorderColor().name() + "; }");

    m_tokenSymbol->setStyleSheet(labelStyle + "QLabel { font-size: 16px; font-weight: bold; }");
    m_tokenName->setStyleSheet(labelStyle + "QLabel { color: " + m_themeManager->subtitleColor().name() + "; font-size: 12px; }");
    m_tokenBalance->setStyleSheet(labelStyle + "QLabel { font-size: 14px; }");
    m_tokenBalanceUSD->setStyleSheet(labelStyle + "QLabel { color: " + m_themeManager->subtitleColor().name() + "; font-size: 12px; }");

    m_deleteButton->setStyleSheet(buttonStyle + "QPushButton { max-width: 30px; max-height: 30px; font-size: 12px; }");
    m_sendButton->setStyleSheet(buttonStyle + "QPushButton { max-width: 30px; max-height: 30px; font-size: 12px; }");
}

TokenCardData QtTokenCard::getTokenData() const {
    return m_tokenData;
}

void QtTokenCard::onCardClicked() {
    emit tokenClicked(m_tokenData.contractAddress);
}

void QtTokenCard::onDeleteClicked() {
    emit deleteTokenClicked(m_tokenData.contractAddress);
}

void QtTokenCard::onSendClicked() {
    emit sendTokenClicked(m_tokenData.contractAddress);
}

void QtTokenCard::onIconDownloaded(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray iconData = reply->readAll();
        QPixmap pixmap;
        if (pixmap.loadFromData(iconData)) {
            m_tokenIcon->setPixmap(pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            setFallbackIcon();
        }
    } else {
        setFallbackIcon();
    }
    reply->deleteLater();
}

void QtTokenCard::onThemeChanged() {
    applyTheme();
}

void QtTokenCard::mousePressEvent(QMouseEvent* event) {
    QFrame::mousePressEvent(event);
    onCardClicked();
}

void QtTokenCard::enterEvent(QEnterEvent* event) {
    QFrame::enterEvent(event);
    m_isHovered = true;
    updateStyles();
}

void QtTokenCard::leaveEvent(QEvent* event) {
    QFrame::leaveEvent(event);
    m_isHovered = false;
    updateStyles();
}

void QtTokenCard::setupUI() {
    setCursor(Qt::PointingHandCursor);

    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(16, 12, 16, 12);
    m_mainLayout->setSpacing(12);

    m_container = new QFrame();
    QHBoxLayout* containerLayout = new QHBoxLayout(m_container);
    containerLayout->setContentsMargins(12, 12, 12, 12);
    containerLayout->setSpacing(12);

    m_tokenIcon = new QLabel();
    m_tokenIcon->setFixedSize(32, 32);
    m_tokenIcon->setScaledContents(true);
    setFallbackIcon();

    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);

    m_tokenSymbol = new QLabel();
    m_tokenSymbol->setWordWrap(false);

    m_tokenName = new QLabel();
    m_tokenName->setWordWrap(false);

    textLayout->addWidget(m_tokenSymbol);
    textLayout->addWidget(m_tokenName);

    QVBoxLayout* balanceLayout = new QVBoxLayout();
    balanceLayout->setSpacing(4);
    balanceLayout->addStretch();

    m_tokenBalance = new QLabel();
    m_tokenBalance->setAlignment(Qt::AlignRight);
    m_tokenBalance->setWordWrap(false);

    m_tokenBalanceUSD = new QLabel();
    m_tokenBalanceUSD->setAlignment(Qt::AlignRight);
    m_tokenBalanceUSD->setWordWrap(false);

    balanceLayout->addWidget(m_tokenBalance);
    balanceLayout->addWidget(m_tokenBalanceUSD);

    QVBoxLayout* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(4);

    m_sendButton = new QPushButton("↑");
    m_sendButton->setFixedSize(30, 30);
    m_sendButton->setToolTip("Send");

    m_deleteButton = new QPushButton("×");
    m_deleteButton->setFixedSize(30, 30);
    m_deleteButton->setToolTip("Remove");

    buttonLayout->addWidget(m_sendButton);
    buttonLayout->addWidget(m_deleteButton);

    containerLayout->addWidget(m_tokenIcon);
    containerLayout->addLayout(textLayout);
    containerLayout->addStretch();
    containerLayout->addLayout(balanceLayout);
    containerLayout->addLayout(buttonLayout);

    m_mainLayout->addWidget(m_container);

    connect(m_deleteButton, &QPushButton::clicked, this, &QtTokenCard::onDeleteClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &QtTokenCard::onSendClicked);
}

void QtTokenCard::updateStyles() {
    if (!m_themeManager) {
        return;
    }

    QString borderColor = m_isHovered ? m_themeManager->accentColor().name() : m_themeManager->defaultBorderColor().name();
    m_container->setStyleSheet("QFrame { background-color: " + m_themeManager->surfaceColor().name() + "; border-radius: " + QString::number(m_themeManager->borderRadiusMedium()) + "px; border: 1px solid " + borderColor + "; }");
}

QString QtTokenCard::getTokenIconUrl(const QString& symbol) {
    QString lowerSymbol = symbol.toLower();
    return QString("https://assets.coingecko.com/coins/images/1/small/%1.png").arg(lowerSymbol);
}

void QtTokenCard::setFallbackIcon() {
    m_tokenIcon->setText("🪙");
    m_tokenIcon->setAlignment(Qt::AlignCenter);
    m_tokenIcon->setStyleSheet("QLabel { font-size: 20px; }");
}
