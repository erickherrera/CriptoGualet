#include "QtTokenListWidget.h"
#include "QtThemeManager.h"
#include <QLineEdit>
#include <QComboBox>
#include <QScrollBar>

QtTokenListWidget::QtTokenListWidget(QtThemeManager* themeManager, QWidget* parent)
    : QWidget(parent)
    , m_themeManager(themeManager)
{
    setupUI();
    applyTheme();
    setEmptyMessage("No tokens imported yet.\nClick 'Import Token' to add your first ERC20 token.");
}

void QtTokenListWidget::addToken(const TokenCardData& tokenData) {
    if (m_tokenCards.contains(tokenData.contractAddress)) {
        updateTokenBalance(tokenData.contractAddress, tokenData.balance, tokenData.balanceUSD);
        return;
    }

    QtTokenCard* card = new QtTokenCard(m_themeManager, this);
    card->setTokenData(tokenData);

    connect(card, &QtTokenCard::tokenClicked, this, &QtTokenListWidget::onTokenClicked);
    connect(card, &QtTokenCard::deleteTokenClicked, this, &QtTokenListWidget::onDeleteTokenClicked);
    connect(card, &QtTokenCard::sendTokenClicked, this, &QtTokenListWidget::onSendTokenClicked);

    m_tokenCards[tokenData.contractAddress] = card;
    m_contractAddresses.append(tokenData.contractAddress);

    m_contentLayout->addWidget(card);
    updateTokenVisibility();
    showTokenList();

    m_countLabel->setText(QString("%1 tokens").arg(m_tokenCards.size()));
}

void QtTokenListWidget::removeToken(const QString& contractAddress) {
    auto it = m_tokenCards.find(contractAddress);
    if (it != m_tokenCards.end()) {
        QtTokenCard* card = it.value();
        m_contentLayout->removeWidget(card);
        m_tokenCards.erase(it);
        m_contractAddresses.removeAll(contractAddress);
        card->deleteLater();
    }

    if (m_tokenCards.isEmpty()) {
        showEmptyState();
    }

    updateTokenVisibility();
    m_countLabel->setText(QString("%1 tokens").arg(m_tokenCards.size()));
}

void QtTokenListWidget::updateTokenBalance(const QString& contractAddress, const QString& balance, const QString& balanceUSD) {
    auto it = m_tokenCards.find(contractAddress);
    if (it != m_tokenCards.end()) {
        QtTokenCard* card = it.value();
        card->setBalance(balance);
        if (!balanceUSD.isEmpty()) {
            card->setBalanceUSD(balanceUSD);
        }
    }
}

void QtTokenListWidget::clearTokens() {
    for (auto card : m_tokenCards) {
        m_contentLayout->removeWidget(card);
        card->deleteLater();
    }
    m_tokenCards.clear();
    m_contractAddresses.clear();
    showEmptyState();
    m_countLabel->setText("0 tokens");
}

void QtTokenListWidget::applyTheme() {
    if (!m_themeManager) {
        return;
    }

    QString baseStyle = m_themeManager->getMainWindowStyleSheet();
    QString labelStyle = m_themeManager->getLabelStyleSheet();
    QString lineEditStyle = m_themeManager->getLineEditStyleSheet();
    QString buttonStyle = m_themeManager->getButtonStyleSheet();

    setStyleSheet(baseStyle);

    m_titleLabel->setStyleSheet(labelStyle + "QLabel { font-size: 18px; font-weight: bold; }");
    m_searchInput->setStyleSheet(lineEditStyle);
    m_sortCombo->setStyleSheet("QComboBox { " + lineEditStyle + " padding: 8px; }");
    m_importButton->setStyleSheet(buttonStyle);
    m_emptyLabel->setStyleSheet(labelStyle + "QLabel { color: " + m_themeManager->subtitleColor().name() + "; text-align: center; }");
    m_countLabel->setStyleSheet(labelStyle + "QLabel { color: " + m_themeManager->subtitleColor().name() + "; font-size: 12px; }");

    m_scrollArea->setStyleSheet("QScrollArea { border: none; background-color: transparent; }");
    m_scrollContent->setStyleSheet("QWidget { background-color: " + m_themeManager->backgroundColor().name() + "; }");

    for (auto card : m_tokenCards) {
        card->applyTheme();
    }
}

void QtTokenListWidget::setEmptyMessage(const QString& message) {
    m_emptyMessage = message;
    m_emptyLabel->setText(message);
}

void QtTokenListWidget::onTokenClicked(const QString& contractAddress) {
    emit tokenClicked(contractAddress);
}

void QtTokenListWidget::onDeleteTokenClicked(const QString& contractAddress) {
    emit deleteTokenClicked(contractAddress);
}

void QtTokenListWidget::onSendTokenClicked(const QString& contractAddress) {
    emit sendTokenClicked(contractAddress);
}

void QtTokenListWidget::onImportClicked() {
    emit importTokenRequested();
}

void QtTokenListWidget::onSearchChanged(const QString& text) {
    (void)text; // Suppress unused parameter warning
    updateTokenVisibility();
}

void QtTokenListWidget::onSortChanged(int index) {
    updateTokenVisibility();
}

void QtTokenListWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(12);

    m_titleLabel = new QLabel("My Tokens");

    QHBoxLayout* controlsLayout = new QHBoxLayout();
    controlsLayout->setSpacing(12);

    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText("Search tokens...");

    m_sortCombo = new QComboBox();
    m_sortCombo->addItem("Sort by: Balance (High to Low)", 0);
    m_sortCombo->addItem("Sort by: Balance (Low to High)", 1);
    m_sortCombo->addItem("Sort by: Name (A-Z)", 2);
    m_sortCombo->addItem("Sort by: Name (Z-A)", 3);

    m_importButton = new QPushButton("Import Token");

    controlsLayout->addWidget(m_searchInput);
    controlsLayout->addWidget(m_sortCombo);
    controlsLayout->addWidget(m_importButton);

    m_scrollArea = new QScrollArea();
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_scrollContent = new QWidget();
    m_contentLayout = new QVBoxLayout(m_scrollContent);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(8);
    m_contentLayout->addStretch();

    m_scrollArea->setWidget(m_scrollContent);

    m_emptyLabel = new QLabel();
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setWordWrap(true);

    m_countLabel = new QLabel();
    m_countLabel->setAlignment(Qt::AlignRight);

    mainLayout->addWidget(m_titleLabel);
    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(m_emptyLabel);
    mainLayout->addWidget(m_scrollArea);
    mainLayout->addWidget(m_countLabel);

    showEmptyState();

    connect(m_importButton, &QPushButton::clicked, this, &QtTokenListWidget::onImportClicked);
    connect(m_searchInput, &QLineEdit::textChanged, this, &QtTokenListWidget::onSearchChanged);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QtTokenListWidget::onSortChanged);
}

void QtTokenListWidget::updateTokenVisibility() {
    QString searchText = m_searchInput->text().trimmed().toLower();
    int sortMode = m_sortCombo->currentIndex();

    QList<QPair<QString, QtTokenCard*>> visibleCards;

    for (auto it = m_tokenCards.begin(); it != m_tokenCards.end(); ++it) {
        QtTokenCard* card = it.value();
        TokenCardData tokenData = card->getTokenData();

        bool matchesSearch = searchText.isEmpty() ||
                            tokenData.symbol.toLower().contains(searchText) ||
                            tokenData.name.toLower().contains(searchText) ||
                            tokenData.contractAddress.toLower().contains(searchText);

        card->setVisible(matchesSearch);
        if (matchesSearch) {
            visibleCards.append(qMakePair(tokenData.contractAddress, card));
        }
    }

    auto compareCards = [sortMode](const QPair<QString, QtTokenCard*>& a, const QPair<QString, QtTokenCard*>& b) {
        TokenCardData dataA = a.second->getTokenData();
        TokenCardData dataB = b.second->getTokenData();

        switch (sortMode) {
            case 0:
                return dataA.balance > dataB.balance;
            case 1:
                return dataA.balance < dataB.balance;
            case 2:
                return dataA.name.toLower() < dataB.name.toLower();
            case 3:
                return dataA.name.toLower() > dataB.name.toLower();
            default:
                return false;
        }
    };

    std::sort(visibleCards.begin(), visibleCards.end(), compareCards);

    for (const auto& pair : visibleCards) {
        QtTokenCard* card = pair.second;
        m_contentLayout->removeWidget(card);
    }

    for (const auto& pair : visibleCards) {
        QtTokenCard* card = pair.second;
        m_contentLayout->insertWidget(m_contentLayout->count() - 1, card);
    }
}

void QtTokenListWidget::showEmptyState() {
    m_emptyLabel->show();
    m_emptyLabel->setText(m_emptyMessage);
    m_scrollArea->hide();
}

void QtTokenListWidget::showTokenList() {
    m_emptyLabel->hide();
    m_scrollArea->show();
}

QString QtTokenListWidget::calculateTokenUSD(const QString& balance, const QString& symbol) {
    // For now, return a placeholder - this would need real price data
    return "$0.00 USD";
}


