#include "QtExpandableWalletCard.h"
#include "QtThemeManager.h"
#include <QEvent>
#include <QMouseEvent>

QtExpandableWalletCard::QtExpandableWalletCard(QtThemeManager *themeManager,
                                               QWidget *parent)
    : QFrame(parent), m_themeManager(themeManager), m_isExpanded(false) {
  setupUI();
  applyTheme();
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

  // Crypto logo container with background
  QWidget *logoContainer = new QWidget(m_collapsedHeader);
  logoContainer->setObjectName("logoContainer");
  logoContainer->setFixedSize(36, 36);

  QHBoxLayout *logoLayout = new QHBoxLayout(logoContainer);
  logoLayout->setContentsMargins(0, 0, 0, 0);
  logoLayout->setAlignment(Qt::AlignCenter);

  m_cryptoLogo = new QLabel("₿", logoContainer);
  m_cryptoLogo->setAlignment(Qt::AlignCenter);
  logoLayout->addWidget(m_cryptoLogo);

  collapsedLayout->addWidget(logoContainer);

  // Balance container
  QWidget *balanceContainer = new QWidget(m_collapsedHeader);
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
  QVBoxLayout *historyLayout = new QVBoxLayout(historySection);
  historyLayout->setContentsMargins(0, 0, 0, 0);
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
  m_cryptoLogo->setText(logoText);
}

void QtExpandableWalletCard::setBalance(const QString &balance) {
  m_balanceLabel->setText(balance);
}

void QtExpandableWalletCard::setTransactionHistory(const QString &historyHtml) {
  m_historyText->setHtml(historyHtml);
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

  // Card styling
  QString walletCardCss = QString(R"(
    QFrame#expandableWalletCard {
      background-color: %1;
      border: 1px solid %2;
      border-radius: 0px;
    }
    QWidget#walletButton {
      background-color: %2;
      border: none;
      border-radius: 0px;
    }
    QWidget#walletButton:hover {
      background-color: %3;
    }
    QWidget#expandedCard {
      background-color: %1;
      border-top: 1px solid %2;
    }
    QWidget#logoContainer {
      background-color: %1;
      border: none;
      border-radius: 0px;
    }
  )")
                              .arg(surface)
                              .arg(accent)
                              .arg(m_themeManager->accentColor().lighter(110).name());

  setStyleSheet(walletCardCss);

  // Logo styling
  QString logoStyle = QString(R"(
    QLabel {
      color: %1;
      font-size: 20px;
      font-weight: bold;
      background: transparent;
      border: none;
    }
  )").arg(text);
  m_cryptoLogo->setStyleSheet(logoStyle);

  // Crypto name styling
  QString nameStyle = QString(R"(
    QLabel {
      color: %1;
      font-size: 10px;
      font-weight: 600;
      letter-spacing: 0.5px;
    }
  )").arg(textSecondary);
  m_cryptoName->setStyleSheet(nameStyle);

  // Balance styling
  QString balanceStyle = QString(R"(
    QLabel {
      color: %1;
      font-size: 13px;
      font-weight: 600;
    }
  )").arg(text);
  m_balanceLabel->setStyleSheet(balanceStyle);

  // Expand indicator styling
  QString indicatorStyle = QString(R"(
    QLabel {
      color: %1;
      font-size: 18px;
      font-weight: bold;
    }
  )").arg(text);
  m_expandIndicator->setStyleSheet(indicatorStyle);

  // Action button styling
  QString buttonStyle = QString(R"(
    QPushButton#actionButton {
      background-color: %1;
      color: %2;
      border: none;
      border-radius: 0px;
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

  // History title styling
  QString historyTitleStyle = QString(R"(
    QLabel {
      color: %1;
      font-size: 14px;
      font-weight: 700;
    }
  )").arg(text);
  m_historyTitleLabel->setStyleSheet(historyTitleStyle);

  // History text area styling
  QString textEditStyle = QString(R"(
    QTextEdit {
      background-color: %1;
      color: %2;
      border: none;
      border-radius: 0px;
      padding: 12px;
      font-family: %3;
      font-size: %4px;
    }
  )")
                            .arg(m_themeManager->surfaceColor().lighter(103).name())
                            .arg(text)
                            .arg(m_themeManager->textFont().family())
                            .arg(m_themeManager->textFont().pointSize());
  m_historyText->setStyleSheet(textEditStyle);
}
