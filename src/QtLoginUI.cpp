// QtLoginUI.cpp
#include "../include/QtLoginUI.h"
#include "../include/Auth.h"
#include "../include/QtThemeManager.h"

#include <QApplication>
#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

QtLoginUI::QtLoginUI(QWidget *parent)
    : QWidget(parent), m_themeManager(&QtThemeManager::instance()) {

  // Initialize message timer
  m_messageTimer = new QTimer(this);
  m_messageTimer->setSingleShot(true);
  connect(m_messageTimer, &QTimer::timeout, this, &QtLoginUI::clearMessage);

  // Load user database on startup
  Auth::LoadUserDatabase();

  setupUI();
  applyTheme();
  connect(m_themeManager, &QtThemeManager::themeChanged, this,
          &QtLoginUI::onThemeChanged);
}

void QtLoginUI::setupUI() {
  // Root layout
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(40, 40, 40, 40);
  m_mainLayout->setSpacing(16);

  // ------ Header (Title + Subtitle) OUTSIDE the card ------
  {
    QWidget *header = new QWidget(this);
    header->setObjectName("loginHeader"); // so we can style-reset this section
    QVBoxLayout *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    m_titleLabel = new QLabel("CriptoGualet", header);
    m_titleLabel->setProperty("class", "title");
    m_titleLabel->setAlignment(Qt::AlignHCenter);

    m_subtitleLabel = new QLabel("Secure Bitcoin Wallet", header);
    m_subtitleLabel->setProperty("class", "subtitle");
    m_subtitleLabel->setAlignment(Qt::AlignHCenter);

    // Kill any frames/backgrounds on labels
    m_titleLabel->setFrameStyle(QFrame::NoFrame);
    m_subtitleLabel->setFrameStyle(QFrame::NoFrame);
    m_titleLabel->setAutoFillBackground(false);
    m_subtitleLabel->setAutoFillBackground(false);
    m_titleLabel->setAttribute(Qt::WA_TranslucentBackground);
    m_subtitleLabel->setAttribute(Qt::WA_TranslucentBackground);

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addWidget(m_subtitleLabel);
    headerLayout->addSpacing(8);

    m_mainLayout->addWidget(header, 0, Qt::AlignHCenter);
  }

  m_mainLayout->addItem(
      new QSpacerItem(20, 12, QSizePolicy::Minimum, QSizePolicy::Expanding));

  createLoginCard();

  m_mainLayout->addItem(
      new QSpacerItem(20, 12, QSizePolicy::Minimum, QSizePolicy::Expanding));

  setupThemeSelector();
}

void QtLoginUI::createLoginCard() {
  m_loginCard = new QFrame(this);
  m_loginCard->setProperty("class", "card");
  m_loginCard->setFixedSize(440, 300);

  m_cardLayout = new QVBoxLayout(m_loginCard);
  m_cardLayout->setContentsMargins(24, 24, 24, 24);
  m_cardLayout->setSpacing(14);

  // ------ Inputs (placeholders only — no captions) ------
  m_usernameEdit = new QLineEdit(m_loginCard);
  m_usernameEdit->setPlaceholderText("Username");
  m_usernameEdit->setMinimumHeight(44);

  // Password field with toggle button container
  QWidget *passwordContainer = new QWidget(m_loginCard);
  passwordContainer->setStyleSheet(
      "QWidget { background-color: transparent; }");
  QHBoxLayout *passwordLayout = new QHBoxLayout(passwordContainer);
  passwordLayout->setContentsMargins(0, 0, 0, 0);
  passwordLayout->setSpacing(0);

  m_passwordEdit = new QLineEdit(passwordContainer);
  m_passwordEdit->setPlaceholderText(
      "Password (6+ chars with letters and numbers)");
  m_passwordEdit->setEchoMode(QLineEdit::Password);
  m_passwordEdit->setMinimumHeight(44);
  m_passwordEdit->setToolTip(
      "Password must contain:\n• At least 6 characters\n• At least one letter "
      "(a-z or A-Z)\n• At least one number (0-9)");

  m_passwordToggleButton = new QPushButton("Show", passwordContainer);
  m_passwordToggleButton->setMinimumSize(50, 44);
  m_passwordToggleButton->setMaximumSize(50, 44);
  m_passwordToggleButton->setCursor(Qt::PointingHandCursor);

  passwordLayout->addWidget(m_passwordEdit);
  passwordLayout->addWidget(m_passwordToggleButton);

  m_cardLayout->addWidget(m_usernameEdit);
  m_cardLayout->addWidget(passwordContainer);

  // Add message label for showing errors/success messages
  m_messageLabel = new QLabel(m_loginCard);
  m_messageLabel->setAlignment(Qt::AlignCenter);
  m_messageLabel->setWordWrap(true);
  m_messageLabel->setMinimumHeight(30);
  m_messageLabel->setStyleSheet(m_themeManager->getMessageStyleSheet());
  m_messageLabel->hide();
  m_cardLayout->addWidget(m_messageLabel);

  m_cardLayout->addSpacing(8);

  // ------ Buttons ------
  m_buttonLayout = new QHBoxLayout();
  m_buttonLayout->setSpacing(12);

  m_loginButton = new QPushButton("Sign In", m_loginCard);
  m_loginButton->setMinimumHeight(44);
  m_loginButton->setMinimumWidth(120);

  m_registerButton = new QPushButton("Create Account", m_loginCard);
  m_registerButton->setMinimumHeight(44);
  m_registerButton->setMinimumWidth(140);

  m_buttonLayout->addWidget(m_loginButton);
  m_buttonLayout->addWidget(m_registerButton);
  m_cardLayout->addLayout(m_buttonLayout);

  // center card horizontally
  QHBoxLayout *cardCenterLayout = new QHBoxLayout();
  cardCenterLayout->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
  cardCenterLayout->addWidget(m_loginCard);
  cardCenterLayout->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  m_mainLayout->addLayout(cardCenterLayout);

  // Signals
  connect(m_loginButton, &QPushButton::clicked, this,
          &QtLoginUI::onLoginClicked);
  connect(m_registerButton, &QPushButton::clicked, this,
          &QtLoginUI::onRegisterClicked);
  connect(m_usernameEdit, &QLineEdit::returnPressed, this,
          &QtLoginUI::onLoginClicked);
  connect(m_passwordEdit, &QLineEdit::returnPressed, this,
          &QtLoginUI::onLoginClicked);
  connect(m_passwordToggleButton, &QPushButton::clicked, this,
          &QtLoginUI::onPasswordVisibilityToggled);
}

void QtLoginUI::setupThemeSelector() {
  m_themeLayout = new QHBoxLayout();
  m_themeLayout->setSpacing(8);
  m_themeLayout->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  QLabel *themeLabel = new QLabel("Theme:");
  themeLabel->setObjectName("themeLabel"); // so it can inherit the reset too
  m_themeLayout->addWidget(themeLabel);

  m_themeSelector = new QComboBox();
  m_themeSelector->addItem("Crypto Dark",
                           static_cast<int>(ThemeType::CRYPTO_DARK));
  m_themeSelector->addItem("Crypto Light",
                           static_cast<int>(ThemeType::CRYPTO_LIGHT));
  m_themeSelector->addItem("Dark", static_cast<int>(ThemeType::DARK));
  m_themeSelector->addItem("Light", static_cast<int>(ThemeType::LIGHT));
  m_themeSelector->setCurrentIndex(0);
  m_themeLayout->addWidget(m_themeSelector);

  m_themeLayout->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
  m_mainLayout->addLayout(m_themeLayout);

  connect(m_themeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [this](int index) {
            ThemeType theme = static_cast<ThemeType>(
                m_themeSelector->itemData(index).toInt());
            m_themeManager->applyTheme(theme);
          });
}

void QtLoginUI::onLoginClicked() {
  const QString username = m_usernameEdit->text().trimmed();
  const QString password = m_passwordEdit->text();

  // Clear any previous messages
  clearMessage();

  if (username.isEmpty() || password.isEmpty()) {
    showMessage("Please enter both username and password", true);
    return;
  }

  // Show processing message
  showMessage("Signing in...", false);

  emit loginRequested(username, password);
}

void QtLoginUI::onRegisterClicked() {
  const QString username = m_usernameEdit->text().trimmed();
  const QString password = m_passwordEdit->text();

  // Clear any previous messages
  clearMessage();

  // Basic validation
  if (username.isEmpty() || password.isEmpty()) {
    showMessage("Please enter both username and password", true);
    return;
  }

  // Quick client-side validation feedback
  if (username.length() < 3) {
    showMessage("Username must be at least 3 characters long", true);
    return;
  }

  if (password.length() < 6) {
    showMessage("Password must be at least 6 characters long", true);
    return;
  }

  // Show processing message
  showMessage("Creating account...", false);

  emit registerRequested(username, password);
}

void QtLoginUI::onLoginResult(bool success, const QString &message) {
  showMessage(message, !success);
  // Always clear password for security
  m_passwordEdit->clear();
}

void QtLoginUI::onRegisterResult(bool success, const QString &message) {
  showMessage(message, !success);
  if (success) {
    // Clear both fields on successful registration
    m_usernameEdit->clear();
    m_passwordEdit->clear();
  } else {
    // Only clear password on failure
    m_passwordEdit->clear();
  }
}

void QtLoginUI::onThemeChanged() { applyTheme(); }
void QtLoginUI::applyTheme() { updateStyles(); }

void QtLoginUI::updateStyles() {
  QString rootCss = m_themeManager->getMainWindowStyleSheet();

  // Force background color same as window for title/subtitle labels
  QColor win = palette().color(QPalette::Window);
  QString winHex = QString("#%1%2%3")
                       .arg(win.red(), 2, 16, QLatin1Char('0'))
                       .arg(win.green(), 2, 16, QLatin1Char('0'))
                       .arg(win.blue(), 2, 16, QLatin1Char('0'));

  // Decide subtitle contrast color depending on brightness
  QColor baseText = palette().color(QPalette::WindowText);
  QColor subtitleColor;
  if (win.value() < 128) {
    // dark background → use lighter gray
    subtitleColor = baseText.lighter(160);
  } else {
    // light background → use darker gray
    subtitleColor = baseText.darker(160);
  }
  QString subtitleHex = QString("#%1%2%3")
                            .arg(subtitleColor.red(), 2, 16, QLatin1Char('0'))
                            .arg(subtitleColor.green(), 2, 16, QLatin1Char('0'))
                            .arg(subtitleColor.blue(), 2, 16, QLatin1Char('0'));

  rootCss += QString(R"(
        QWidget#loginHeader, QWidget#loginHeader * {
            border: none !important;
            outline: none !important;
        }
        QLabel[class="title"] {
            font-size: 40px;
            font-weight: 700;
            letter-spacing: 0.2px;
        }
        QLabel[class="subtitle"] {
            color: %2;
            font-size: 20px;
            font-weight: 400;
            margin-top: 4px;
        }
        QLabel#themeLabel {
            background-color: %1 !important;
            border: none !important;
        }
    )")
                 .arg(winHex, subtitleHex);

  setStyleSheet(rootCss);

  // Card
  const QString cardCss = m_themeManager->getCardStyleSheet() + R"(
        QFrame {
            border-width: 2px;
            border-style: solid;
            border-radius: 12px;
        }
    )";
  m_loginCard->setStyleSheet(cardCss);

  // Inputs
  QString lineEditStyle = m_themeManager->getLineEditStyleSheet();
  lineEditStyle += R"(
        QLineEdit {
            border-radius: 8px;
            min-height: 44px;
            padding: 0 10px;
            font-size: 14px;
        }
        QLineEdit:focus {
            border: 1.5px solid palette(highlight);
        }
    )";
  m_usernameEdit->setStyleSheet(lineEditStyle);

  // Password field - keep all corners rounded
  m_passwordEdit->setStyleSheet(lineEditStyle);

  // Buttons
  QString buttonStyle = m_themeManager->getButtonStyleSheet();
  buttonStyle += R"(
        QPushButton { font-size: 14px; padding: 0 18px; border-radius: 8px; }
    )";
  m_loginButton->setStyleSheet(buttonStyle);
  m_registerButton->setStyleSheet(buttonStyle);

  // Password toggle button
  QString toggleButtonStyle = R"(
        QPushButton { 
            font-size: 12px; 
            border: none; 
            border-radius: 8px;
            background-color: transparent;
            color: %1;
            font-weight: 500;
        }
        QPushButton:hover { 
            color: %2;
            text-decoration: underline;
        }
    )";
  m_passwordToggleButton->setStyleSheet(
      toggleButtonStyle.arg(m_themeManager->textColor().name())
          .arg(m_themeManager->accentColor().name()));

  // Fonts
  QFont titleF = m_themeManager->titleFont();
  titleF.setPointSizeF(titleF.pointSizeF() + 6);
  m_titleLabel->setFont(titleF);
  QFont subtitleF = m_themeManager->textFont();
  subtitleF.setPointSizeF(subtitleF.pointSizeF() + 2);
  m_subtitleLabel->setFont(subtitleF);
  m_loginButton->setFont(m_themeManager->buttonFont());
  m_registerButton->setFont(m_themeManager->buttonFont());
  m_usernameEdit->setFont(m_themeManager->textFont());
  m_passwordEdit->setFont(m_themeManager->textFont());
}

void QtLoginUI::showMessage(const QString &message, bool isError) {
  if (m_messageLabel) {
    m_messageLabel->setText(message);
    m_messageLabel->setProperty("isError", isError);

    // Apply styling based on error state
    if (isError) {
      m_messageLabel->setStyleSheet(
          m_themeManager->getErrorMessageStyleSheet());
    } else {
      m_messageLabel->setStyleSheet(
          m_themeManager->getSuccessMessageStyleSheet());
    }

    m_messageLabel->show();

    // Auto-clear after 5 seconds
    m_messageTimer->start(5000);
  }
}

void QtLoginUI::clearMessage() {
  if (m_messageLabel) {
    m_messageLabel->clear();
    m_messageLabel->hide();
  }
}

void QtLoginUI::onPasswordVisibilityToggled() {
  if (m_passwordEdit->echoMode() == QLineEdit::Password) {
    m_passwordEdit->setEchoMode(QLineEdit::Normal);
    m_passwordToggleButton->setText("Hide");
  } else {
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordToggleButton->setText("Show");
  }
}
