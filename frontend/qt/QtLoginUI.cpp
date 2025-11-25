// QtLoginUI.cpp
#include "QtLoginUI.h"
#include "Auth.h"
#include "QtEmailVerificationDialog.h"
#include "QtThemeManager.h"

#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>

#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>
#include <functional>
#include <memory>
#include <optional>

QtLoginUI::QtLoginUI(QWidget *parent)
    : QWidget(parent), m_themeManager(&QtThemeManager::instance()) {
  // Initialize message timer
  m_messageTimer = new QTimer(this);
  m_messageTimer->setSingleShot(true);
  connect(m_messageTimer, &QTimer::timeout, this, &QtLoginUI::clearMessage);

  // Initialize Auth database and Repository layer on startup
  if (!Auth::InitializeAuthDatabase()) {
    // Database initialization failed - log warning but allow app to continue
    // Users can still be created in-memory for this session
    qWarning() << "Failed to initialize authentication database. Data will not "
                  "be persisted.";
  }

  setupUI();
  applyTheme();

  connect(m_themeManager, &QtThemeManager::themeChanged, this,
          &QtLoginUI::onThemeChanged);
}

void QtLoginUI::setupUI() {
  // Root layout
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(
      m_themeManager->standardMargin(),
      m_themeManager->generousMargin(),
      m_themeManager->standardMargin(),
      m_themeManager->generousMargin()
  );
  m_mainLayout->setSpacing(m_themeManager->compactSpacing());

  // ------ Header (Title + Subtitle) OUTSIDE the card ------
  {
    QWidget *header = new QWidget(this);
    header->setObjectName("loginHeader");
    QVBoxLayout *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    m_titleLabel = new QLabel("CriptoGualet", header);
    m_titleLabel->setProperty("class", "title");
    m_titleLabel->setAlignment(Qt::AlignHCenter);

    m_subtitleLabel = new QLabel("A Secure Crypto Wallet", header);
    m_subtitleLabel->setProperty("class", "subtitle");
    m_subtitleLabel->setAlignment(Qt::AlignHCenter);

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
}

void QtLoginUI::createLoginCard() {
  m_loginCard = new QFrame(this);
  m_loginCard->setProperty("class", "card");
  m_loginCard->setMinimumSize(380, 420);
  m_loginCard->setMaximumSize(520, 500);
  m_loginCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  m_cardLayout = new QVBoxLayout(m_loginCard);
  m_cardLayout->setContentsMargins(20, 20, 20, 16);
  m_cardLayout->setSpacing(0); // No spacing for unified look

  // Create custom tab system with centered tab bar
  // Tab bar
  m_tabBar = new QTabBar(m_loginCard);
  m_tabBar->setExpanding(false);
  m_tabBar->setUsesScrollButtons(false);
  m_tabBar->setDrawBase(false);
  m_tabBar->setDocumentMode(true);

  // Create centered tab bar container
  QWidget *tabBarContainer = new QWidget(m_loginCard);
  QHBoxLayout *tabBarLayout = new QHBoxLayout(tabBarContainer);
  tabBarLayout->setContentsMargins(0, 0, 0, 25);
  tabBarLayout->setSpacing(0);
  tabBarLayout->addStretch();
  tabBarLayout->addWidget(m_tabBar);
  tabBarLayout->addStretch();

  m_cardLayout->addWidget(tabBarContainer);

  // Stacked widget for tab content - no margins for unified look
  m_stackedWidget = new QStackedWidget(m_loginCard);
  m_stackedWidget->setContentsMargins(0, 0, 0, 0);
  m_cardLayout->addWidget(m_stackedWidget, 1); // Stretch to fill available space

  // Connect tab bar to stacked widget
  connect(m_tabBar, &QTabBar::currentChanged, m_stackedWidget, &QStackedWidget::setCurrentIndex);

  // ===== Sign In Tab =====
  QWidget *signInTab = new QWidget();
  QVBoxLayout *signInLayout = new QVBoxLayout(signInTab);
  signInLayout->setContentsMargins(24, 4, 24, 20); // Reduced top margin for unified look
  signInLayout->setSpacing(10);
  signInLayout->setAlignment(Qt::AlignCenter);

  m_loginUsernameEdit = new QLineEdit(signInTab);
  m_loginUsernameEdit->setPlaceholderText("Username");
  m_loginUsernameEdit->setMinimumHeight(44);

  m_loginPasswordEdit = new QLineEdit(signInTab);
  m_loginPasswordEdit->setPlaceholderText("Password");
  m_loginPasswordEdit->setEchoMode(QLineEdit::Password);
  m_loginPasswordEdit->setMinimumHeight(44);

  // Create password toggle button for login
  m_loginPasswordToggleButton = new QPushButton("Show", m_loginPasswordEdit);
  m_loginPasswordToggleButton->setMinimumSize(50, 30);
  m_loginPasswordToggleButton->setMaximumSize(50, 30);
  m_loginPasswordToggleButton->setCursor(Qt::PointingHandCursor);
  m_loginPasswordToggleButton->setFlat(true);

  // Position the button inside the password field on the right side
  auto positionLoginToggle = [this]() {
    if (!m_loginPasswordToggleButton || !m_loginPasswordEdit)
      return;
    const int buttonWidth = m_loginPasswordToggleButton->width();
    const int padding = 8;
    m_loginPasswordEdit->setTextMargins(0, 0, buttonWidth + padding, 0);
    const int x = m_loginPasswordEdit->width() - buttonWidth - padding;
    const int y = (m_loginPasswordEdit->height() -
                   m_loginPasswordToggleButton->height()) /
                  2;
    m_loginPasswordToggleButton->move(x, y);
  };

  connect(m_loginPasswordEdit, &QLineEdit::textChanged, this,
          positionLoginToggle);
  m_loginPasswordEdit->installEventFilter(this);
  QTimer::singleShot(0, this, positionLoginToggle);

  m_loginButton = new QPushButton("Sign In", signInTab);
  m_loginButton->setMinimumHeight(44);

  signInLayout->addWidget(m_loginUsernameEdit);
  signInLayout->addWidget(m_loginPasswordEdit);
  signInLayout->addSpacing(8);
  signInLayout->addWidget(m_loginButton);
  signInLayout->addStretch();

  m_tabBar->addTab("Sign In");
  m_stackedWidget->addWidget(signInTab);

  // ===== Register Tab =====
  QWidget *registerTab = new QWidget();
  QVBoxLayout *registerLayout = new QVBoxLayout(registerTab);
  registerLayout->setContentsMargins(24, 4, 24, 20); // Reduced top margin for unified look
  registerLayout->setSpacing(10);
  registerLayout->setAlignment(Qt::AlignCenter);

  m_usernameEdit = new QLineEdit(registerTab);
  m_usernameEdit->setPlaceholderText("Username");
  m_usernameEdit->setMinimumHeight(44);

  m_emailEdit = new QLineEdit(registerTab);
  m_emailEdit->setPlaceholderText("Email Address");
  m_emailEdit->setMinimumHeight(44);

  m_passwordEdit = new QLineEdit(registerTab);
  m_passwordEdit->setPlaceholderText(
      "Password (6+ chars with letters and numbers)");
  m_passwordEdit->setEchoMode(QLineEdit::Password);
  m_passwordEdit->setMinimumHeight(44);
  m_passwordEdit->setToolTip(
      "Password must contain:\n• At least 6 characters\n• At least one "
      "letter\n• At least one "
      "number");

  // Create password toggle button for register
  m_passwordToggleButton = new QPushButton("Show", m_passwordEdit);
  m_passwordToggleButton->setMinimumSize(50, 30);
  m_passwordToggleButton->setMaximumSize(50, 30);
  m_passwordToggleButton->setCursor(Qt::PointingHandCursor);
  m_passwordToggleButton->setFlat(true);

  // Position the button inside the password field on the right side
  auto positionRegisterToggle = [this]() {
    if (!m_passwordToggleButton || !m_passwordEdit)
      return;
    const int buttonWidth = m_passwordToggleButton->width();
    const int padding = 8;
    m_passwordEdit->setTextMargins(0, 0, buttonWidth + padding, 0);
    const int x = m_passwordEdit->width() - buttonWidth - padding;
    const int y =
        (m_passwordEdit->height() - m_passwordToggleButton->height()) / 2;
    m_passwordToggleButton->move(x, y);
  };

  connect(m_passwordEdit, &QLineEdit::textChanged, this,
          positionRegisterToggle);
  m_passwordEdit->installEventFilter(this);
  QTimer::singleShot(0, this, positionRegisterToggle);

  m_registerButton = new QPushButton("Register", registerTab);
  m_registerButton->setMinimumHeight(44);
  m_registerButton->setEnabled(false); // Disabled until all fields are filled

  registerLayout->addWidget(m_usernameEdit);
  registerLayout->addWidget(m_emailEdit);
  registerLayout->addWidget(m_passwordEdit);
  registerLayout->addSpacing(8);
  registerLayout->addWidget(m_registerButton);
  registerLayout->addStretch();

  m_tabBar->addTab("Register");
  m_stackedWidget->addWidget(registerTab);

  m_cardLayout->addSpacing(8); // Small space between tabs and message

  // Message label (shared between tabs)
  m_messageLabel = new QLabel(m_loginCard);
  m_messageLabel->setAlignment(Qt::AlignCenter);
  m_messageLabel->setWordWrap(true);
  m_messageLabel->setMinimumHeight(30);
  m_messageLabel->setStyleSheet(m_themeManager->getMessageStyleSheet());
  m_messageLabel->hide();
  m_cardLayout->addWidget(m_messageLabel);

  m_cardLayout->addSpacing(8); // Small space between message and secondary buttons

  // ------ Secondary actions (Reveal / Restore) ------
  QHBoxLayout *secondary = new QHBoxLayout();
  secondary->setSpacing(8);

  m_revealSeedButton = new QPushButton("Reveal Seed (re-auth)", m_loginCard);
  m_revealSeedButton->setMinimumHeight(36);
  m_restoreSeedButton = new QPushButton("Restore from Seed", m_loginCard);
  m_restoreSeedButton->setMinimumHeight(36);

  secondary->addWidget(m_revealSeedButton);
  secondary->addWidget(m_restoreSeedButton);
  m_cardLayout->addLayout(secondary);

  // Center card horizontally
  QHBoxLayout *cardCenterLayout = new QHBoxLayout();
  cardCenterLayout->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));
  cardCenterLayout->addWidget(m_loginCard);
  cardCenterLayout->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  m_mainLayout->addLayout(cardCenterLayout);

  // Signals - Sign In tab
  connect(m_loginButton, &QPushButton::clicked, this,
          &QtLoginUI::onLoginClicked);
  connect(m_loginUsernameEdit, &QLineEdit::returnPressed, this,
          &QtLoginUI::onLoginClicked);
  connect(m_loginPasswordEdit, &QLineEdit::returnPressed, this,
          &QtLoginUI::onLoginClicked);
  connect(m_loginPasswordToggleButton, &QPushButton::clicked, this, [this]() {
    if (m_loginPasswordEdit->echoMode() == QLineEdit::Password) {
      m_loginPasswordEdit->setEchoMode(QLineEdit::Normal);
      m_loginPasswordToggleButton->setText("Hide");
    } else {
      m_loginPasswordEdit->setEchoMode(QLineEdit::Password);
      m_loginPasswordToggleButton->setText("Show");
    }
  });

  // Signals - Register tab
  connect(m_registerButton, &QPushButton::clicked, this,
          &QtLoginUI::onRegisterClicked);
  connect(m_usernameEdit, &QLineEdit::textChanged, this,
          &QtLoginUI::validateRegisterForm);
  connect(m_emailEdit, &QLineEdit::textChanged, this,
          &QtLoginUI::validateRegisterForm);
  connect(m_passwordEdit, &QLineEdit::textChanged, this,
          &QtLoginUI::validateRegisterForm);
  connect(m_usernameEdit, &QLineEdit::returnPressed, this,
          &QtLoginUI::onRegisterClicked);
  connect(m_emailEdit, &QLineEdit::returnPressed, this,
          &QtLoginUI::onRegisterClicked);
  connect(m_passwordEdit, &QLineEdit::returnPressed, this,
          &QtLoginUI::onRegisterClicked);
  connect(m_passwordToggleButton, &QPushButton::clicked, this,
          &QtLoginUI::onPasswordVisibilityToggled);

  // Tab changed signal - clear messages when switching tabs
  connect(m_tabBar, &QTabBar::currentChanged, this,
          [this]() { clearMessage(); });

  // Secondary actions
  connect(m_revealSeedButton, &QPushButton::clicked, this,
          &QtLoginUI::onRevealSeedClicked);
  connect(m_restoreSeedButton, &QPushButton::clicked, this,
          &QtLoginUI::onRestoreSeedClicked);
}

void QtLoginUI::onLoginClicked() {
  const QString username = m_loginUsernameEdit->text().trimmed();
  const QString password = m_loginPasswordEdit->text();

  clearMessage();

  if (username.isEmpty() || password.isEmpty()) {
    showMessage("Please enter both username and password", true);
    return;
  }

  showMessage("Signing in...", false);
  emit loginRequested(username, password);
}

void QtLoginUI::onRegisterClicked() {
  const QString username = m_usernameEdit->text().trimmed();
  const QString email = m_emailEdit->text().trimmed();
  const QString password = m_passwordEdit->text();

  clearMessage();

  if (username.isEmpty() || email.isEmpty() || password.isEmpty()) {
    showMessage("Please enter username, email, and password", true);
    return;
  }
  if (username.length() < 3) {
    showMessage("Username must be at least 3 characters long", true);
    return;
  }
  if (!email.contains("@") || !email.contains(".")) {
    showMessage("Please enter a valid email address", true);
    return;
  }
  if (password.length() < 6) {
    showMessage("Password must be at least 6 characters long", true);
    return;
  }

  showMessage("Creating account... generating your seed phrase securely.",
              false);
  emit registerRequested(username, email, password);
}

void QtLoginUI::onRegisterModeToggled(bool registerMode) {
  if (registerMode) {
    m_emailEdit->show();
    m_registerButton->setText("Register");
    m_loginButton->setText("Back to Login");
    clearMessage();
  } else {
    m_emailEdit->hide();
    m_registerButton->setText("Create Account");
    m_loginButton->setText("Sign In");
    clearMessage();
  }

  // Reconnect login button based on mode
  disconnect(m_loginButton, &QPushButton::clicked, nullptr, nullptr);
  if (registerMode) {
    connect(m_loginButton, &QPushButton::clicked, this,
            [this]() { m_registerButton->setChecked(false); });
  } else {
    connect(m_loginButton, &QPushButton::clicked, this,
            &QtLoginUI::onLoginClicked);
  }
}

void QtLoginUI::onLoginResult(bool success, const QString &message) {
  // Check if login failed due to unverified email
  // EMAIL_NOT_VERIFIED message only appears when:
  // 1. User exists in database (authentication succeeded)
  // 2. Password is correct (authentication succeeded)
  // 3. Email is not verified
  // 
  // For non-existent users, the backend returns "Invalid credentials" message,
  // NOT "EMAIL_NOT_VERIFIED"
  // 
  // The backend formats the message as "EMAIL_NOT_VERIFIED: ..." so we check
  // if the message starts with this exact prefix
  if (!success && message.startsWith("EMAIL_NOT_VERIFIED:", Qt::CaseSensitive)) {
    // User exists and password is correct, but email is not verified
    // Extract username from login form
    const QString username = m_loginUsernameEdit->text().trimmed();

    // Show verification dialog
    showMessage("Your email address has not been verified. Please verify to continue.", true);

    // Small delay to let user read the message
    QTimer::singleShot(1500, this, [this, username]() {
      // Show email verification dialog
      QtEmailVerificationDialog verifyDlg(username, "", this);

      if (verifyDlg.exec() == QDialog::Accepted && verifyDlg.isVerified()) {
        // Email verified! User can now try logging in again
        showMessage("Email verified successfully! Please sign in.", false);
        m_loginPasswordEdit->setFocus();
      } else {
        showMessage("Email verification incomplete. Please verify your email to sign in.", true);
      }
    });
  } else {
    // Not an email verification issue - show the message as-is
    // For non-existent users, this will show "Invalid credentials" message
    showMessage(message, !success);
  }

  m_loginPasswordEdit->clear();
}

void QtLoginUI::onRegisterResult(bool success, const QString &message) {
  showMessage(message, !success);

  if (success) {
    const QString username = m_usernameEdit->text().trimmed();
    
    // Clear registration fields
    m_usernameEdit->clear();
    m_emailEdit->clear();
    m_passwordEdit->clear();

    // Switch to Sign In tab if email was verified
    if (message.contains("verified", Qt::CaseInsensitive)) {
      m_tabBar->setCurrentIndex(0);
      // Pre-fill username in login form for convenience
      m_loginUsernameEdit->setText(username);
    }
  } else {
    // Registration failed - clear password but keep username and email for retry
    m_passwordEdit->clear();
  }
}

void QtLoginUI::onRevealSeedClicked() {
  const QString username = m_usernameEdit->text().trimmed();
  if (username.isEmpty()) {
    showMessage("Enter your username first.", true);
    return;
  }

  // Re-auth dialog (password prompt)
  QDialog authDlg(this);
  authDlg.setWindowTitle("Re-authenticate");
  authDlg.setModal(true);

  QGridLayout *grid = new QGridLayout(&authDlg);
  grid->setContentsMargins(16, 16, 16, 16);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(8);

  QLabel *lbl =
      new QLabel("Enter your password to reveal your seed:", &authDlg);
  QLineEdit *pwd = new QLineEdit(&authDlg);
  pwd->setEchoMode(QLineEdit::Password);
  pwd->setText(m_passwordEdit->text()); // prefill if user typed it already

  QDialogButtonBox *box =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                           Qt::Horizontal, &authDlg);

  int row = 0;
  grid->addWidget(lbl, row++, 0, 1, 2);
  grid->addWidget(pwd, row++, 0, 1, 2);
  grid->addWidget(box, row++, 0, 1, 2);

  QObject::connect(box, &QDialogButtonBox::accepted, &authDlg,
                   &QDialog::accept);
  QObject::connect(box, &QDialogButtonBox::rejected, &authDlg,
                   &QDialog::reject);

  if (authDlg.exec() != QDialog::Accepted)
    return;

  // Call Auth to verify & pull seed (Qt-free core: std::optional<std::string>)
  std::string seedHex;
  std::optional<std::string> mnemonic;
  auto resp = Auth::RevealSeed(username.toStdString(),
                               pwd->text().toStdString(), seedHex,
                               /*outMnemonic*/ mnemonic);

  if (resp.result != Auth::AuthResult::SUCCESS) {
    showMessage(QString::fromStdString(resp.message), true);
    return;
  }

  // Show seed (and mnemonic if available) with consent gating + copy guards
  QDialog reveal(this);
  reveal.setWindowTitle("Your Seed");
  reveal.setModal(true);

  QGridLayout *lay = new QGridLayout(&reveal);
  lay->setContentsMargins(16, 16, 16, 16);
  lay->setHorizontalSpacing(8);
  lay->setVerticalSpacing(10);

  QLabel *warn =
      new QLabel("<b>Anyone with this can access your wallet.</b><br/>"
                 "Do not share or screenshot this screen.",
                 &reveal);
  warn->setWordWrap(true);

  // Seed section (disabled until user consents)
  QCheckBox *showSeed =
      new QCheckBox("I understand the risks. Show my seed now.", &reveal);
  QLabel *seedLbl = new QLabel("BIP-39 Seed (64 bytes, hex):", &reveal);
  QPlainTextEdit *seedBox = new QPlainTextEdit(&reveal);
  seedBox->setReadOnly(true);
  seedBox->setMaximumHeight(80);
  seedBox->setPlainText(QString::fromStdString(seedHex));
  seedLbl->setEnabled(false);
  seedBox->setEnabled(false);

  QPushButton *copySeed =
      new QPushButton("Copy Seed (auto-clears in 30s)", &reveal);
  copySeed->setEnabled(false);

  // Mnemonic section (only if one-time file still exists)
  QCheckBox *showWords = nullptr;
  QLabel *mnemoLbl = nullptr;
  QPlainTextEdit *mnemoBox = nullptr;
  QPushButton *copyMnemonic = nullptr;

  if (mnemonic.has_value()) {
    showWords = new QCheckBox(
        "Also show my 12/24 words from the one-time backup file.", &reveal);
    mnemoLbl = new QLabel("Mnemonic:", &reveal);
    mnemoBox = new QPlainTextEdit(&reveal);
    mnemoBox->setReadOnly(true);
    mnemoBox->setMaximumHeight(80);
    mnemoBox->setPlainText(QString::fromStdString(*mnemonic));
    mnemoLbl->setEnabled(false);
    mnemoBox->setEnabled(false);
    copyMnemonic = new QPushButton("Copy Words (auto-clears in 30s)", &reveal);
    copyMnemonic->setEnabled(false);
  }

  QDialogButtonBox *box2 =
      new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, &reveal);

  int r = 0;
  lay->addWidget(warn, r++, 0, 1, 2);
  lay->addWidget(showSeed, r++, 0, 1, 2);
  lay->addWidget(seedLbl, r++, 0, 1, 2);
  lay->addWidget(seedBox, r++, 0, 1, 2);
  lay->addWidget(copySeed, r++, 0, 1, 2);

  if (showWords) {
    lay->addWidget(showWords, r++, 0, 1, 2);
    lay->addWidget(mnemoLbl, r++, 0, 1, 2);
    lay->addWidget(mnemoBox, r++, 0, 1, 2);
    lay->addWidget(copyMnemonic, r++, 0, 1, 2);
  }

  lay->addWidget(box2, r++, 0, 1, 2);

  auto copyWithAutoClear = [&](const QString &text) {
    QClipboard *cb = QGuiApplication::clipboard();
    cb->setText(text);
    QTimer::singleShot(30000, this, [cb]() { cb->clear(); }); // clear in 30s
    QMessageBox::information(
        this, "Copied",
        "Copied to clipboard. It will be cleared in 30 seconds.");
  };

  // Enable/disable sections based on user consent
  QObject::connect(showSeed, &QCheckBox::toggled, this, [&](bool on) {
    seedLbl->setEnabled(on);
    seedBox->setEnabled(on);
    copySeed->setEnabled(on);
    if (!on) {
      // Extra precaution: clear clipboard if it contains this text
      QClipboard *cb = QGuiApplication::clipboard();
      if (cb->text() == seedBox->toPlainText())
        cb->clear();
    }
  });
  if (showWords) {
    QObject::connect(showWords, &QCheckBox::toggled, this, [&](bool on) {
      mnemoLbl->setEnabled(on);
      mnemoBox->setEnabled(on);
      copyMnemonic->setEnabled(on);
      if (!on) {
        QClipboard *cb = QGuiApplication::clipboard();
        if (cb->text() == mnemoBox->toPlainText())
          cb->clear();
      }
    });
  }

  QObject::connect(copySeed, &QPushButton::clicked, this, [&, seedBox]() {
    copyWithAutoClear(seedBox->toPlainText());
  });
  if (copyMnemonic) {
    QObject::connect(
        copyMnemonic, &QPushButton::clicked, this,
        [&, mnemoBox]() { copyWithAutoClear(mnemoBox->toPlainText()); });
  }
  QObject::connect(box2, &QDialogButtonBox::rejected, &reveal,
                   &QDialog::reject);
  QObject::connect(box2, &QDialogButtonBox::accepted, &reveal,
                   &QDialog::accept);

  // Clear clipboard on close if it still contains our sensitive text
  QObject::connect(&reveal, &QDialog::finished, this, [&, seedBox, mnemoBox]() {
    QClipboard *cb = QGuiApplication::clipboard();
    const QString t = cb->text();
    if (t == seedBox->toPlainText() ||
        (mnemoBox && t == mnemoBox->toPlainText())) {
      cb->clear();
    }
  });

  reveal.exec();
}

void QtLoginUI::onRestoreSeedClicked() {
  const QString username = m_usernameEdit->text().trimmed();
  if (username.isEmpty()) {
    showMessage("Enter your username first.", true);
    return;
  }

  // Require password to prevent unauthorized overwrite
  QDialog dlg(this);
  dlg.setWindowTitle("Restore from Seed");
  dlg.setModal(true);

  QGridLayout *grid = new QGridLayout(&dlg);
  grid->setContentsMargins(16, 16, 16, 16);
  grid->setHorizontalSpacing(8);
  grid->setVerticalSpacing(8);

  QLabel *info = new QLabel("Paste your 12 or 24 BIP-39 words (single line or "
                            "spaced). Optional: BIP39 passphrase.",
                            &dlg);
  info->setWordWrap(true);

  QLabel *pwdLbl = new QLabel("Confirm your account password:", &dlg);
  QLineEdit *pwd = new QLineEdit(&dlg);
  pwd->setEchoMode(QLineEdit::Password);
  pwd->setText(m_passwordEdit->text());

  QLabel *mnemoLbl = new QLabel("Mnemonic words:", &dlg);
  QPlainTextEdit *mnemo = new QPlainTextEdit(&dlg);
  mnemo->setPlaceholderText("example: ladder merry ... (12 or 24 words)");

  QLabel *passLbl = new QLabel("BIP39 passphrase (optional):", &dlg);
  QLineEdit *passphrase = new QLineEdit(&dlg);
  passphrase->setEchoMode(QLineEdit::Normal);

  QDialogButtonBox *box = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dlg);

  int row = 0;
  grid->addWidget(info, row++, 0, 1, 2);
  grid->addWidget(pwdLbl, row++, 0, 1, 2);
  grid->addWidget(pwd, row++, 0, 1, 2);
  grid->addWidget(mnemoLbl, row++, 0, 1, 2);
  grid->addWidget(mnemo, row++, 0, 1, 2);
  grid->addWidget(passLbl, row++, 0, 1, 2);
  grid->addWidget(passphrase, row++, 0, 1, 2);
  grid->addWidget(box, row++, 0, 1, 2);

  QObject::connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
  QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

  if (dlg.exec() != QDialog::Accepted)
    return;

  // Call Auth: verify password, then restore
  const auto resp = Auth::RestoreFromSeed(username.toStdString(),
                                          mnemo->toPlainText().toStdString(),
                                          passphrase->text().toStdString(),
                                          pwd->text().toStdString() // re-auth
  );

  if (resp.result == Auth::AuthResult::SUCCESS) {
    QMessageBox::information(
        this, "Seed Restored",
        "Your seed has been restored and stored securely.");
  } else {
    showMessage(QString::fromStdString(resp.message), true);
  }
}

void QtLoginUI::onThemeChanged() { applyTheme(); }
void QtLoginUI::applyTheme() { updateStyles(); }

void QtLoginUI::updateStyles() {
  QString appBg = m_themeManager->backgroundColor().name(); // Use background for main window
  QString cardBg = m_themeManager->surfaceColor().name(); // Use surface color for cards
  QString textHex = m_themeManager->textColor().name();
  QString accentHex = m_themeManager->accentColor().name();

  QColor win = m_themeManager->backgroundColor();
  QString winHex = win.name();

  QColor baseText = m_themeManager->textColor();
  QColor base = m_themeManager->surfaceColor();
  QString baseHex = base.name();

  QColor subtitleColor = m_themeManager->subtitleColor();
  QString subtitleHex = subtitleColor.name();

  QString borderColor = m_themeManager->secondaryColor().name();

  // Set main widget background explicitly - be specific to avoid overriding children
  QString rootCss = QString(R"(
        QtLoginUI {
            background-color: %1;
        }
        QWidget#loginHeader, QWidget#loginHeader * {
            border: none !important;
            outline: none !important;
            background: transparent;
        }
        QLabel[class="title"] {
            font-size: 40px;
            font-weight: 700;
            letter-spacing: 0.2px;
            background: transparent;
            color: %3;
        }
        QLabel[class="subtitle"] {
            color: %2;
            font-size: 20px;
            font-weight: 400;
            margin-top: 4px;
            background: transparent;
        }
    )")
                 .arg(appBg, subtitleHex, textHex);

  setStyleSheet(rootCss);

  // Set widget palette to ensure background color is applied
  QPalette pal = palette();
  pal.setColor(QPalette::Window, m_themeManager->backgroundColor());
  pal.setColor(QPalette::Base, m_themeManager->surfaceColor());
  pal.setColor(QPalette::WindowText, m_themeManager->textColor());
  setPalette(pal);
  setAutoFillBackground(true);

  // Enhanced card styling with proper background
  // IMPORTANT: All children must be transparent so card background shows through
  const QString cardCss = QString(R"(
        QFrame[class="card"] {
            background-color: %1;
            border: 2px solid %2;
            border-radius: 12px;
        }
        QFrame[class="card"] > QWidget {
            background-color: transparent;
        }
        QFrame[class="card"] QStackedWidget {
            background-color: transparent;
        }
        QFrame[class="card"] QStackedWidget > QWidget {
            background-color: transparent;
        }
    )")
                              .arg(cardBg, borderColor);
  m_loginCard->setStyleSheet(cardCss);

  // Tab Bar Styling - Unified with content below, no gap
  // Both tabs are fully readable, bottom border indicates selection
  QString inactiveTabColor = m_themeManager->dimmedTextColor().name();  // Theme-aware dimmed text
  QString selectedTabColor = m_themeManager->textColor().name(); // Always use theme text color for selected tab

  QString tabBarStyle =
      QString(R"(
        QTabBar {
            background: transparent;
            border: none;
            border-bottom: 1px solid %3;
        }
        QTabBar::tab {
            background: transparent;
            color: %6;
            padding: 10px 24px;
            margin-left: 4px;
            margin-right: 4px;
            margin-bottom: 0px;
            margin-top: 0px;
            border: none;
            border-bottom: 2px solid transparent;
            font-size: 14px;
            font-weight: 500;
        }
        QTabBar::tab:selected {
            background: transparent;
            color: %7;
            border-bottom: 2px solid %4;
            font-weight: 600;
        }
        QTabBar::tab:selected:hover {
            background: transparent;
            color: %7;
            border-bottom: 2px solid %4;
            font-weight: 600;
        }
        QTabBar::tab:hover:!selected {
            background: transparent;
            color: %4;
            border-bottom: 2px solid transparent;
        }
    )")
          .arg(baseHex, textHex, borderColor, accentHex, cardBg,
               inactiveTabColor, selectedTabColor);  // %7 - explicit selected color
  m_tabBar->setStyleSheet(tabBarStyle);

  // Enhanced LineEdit styling with proper backgrounds and contrast
  // Input fields should be darker than card background in dark mode, lighter in light mode
  QString inputBg = m_themeManager->surfaceColor().name();  // Theme-aware input background

  QString lineEditStyle = QString(R"(
        QLineEdit {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 8px;
            min-height: 44px;
            padding: 0 10px;
            font-size: 14px;
            selection-background-color: %4;
        }
        QLineEdit::placeholder {
            color: %8;
        }
        QLineEdit:focus {
            border: 2px solid %4;
            background-color: %1;
        }
        QLineEdit:hover {
            border: 1px solid %5;
        }
        QLineEdit:disabled {
            background-color: %6;
            color: %7;
        }
    )")
                              .arg(inputBg, textHex, borderColor, accentHex,
                                   m_themeManager->focusBorderColor().name(),         // hover border
                                   m_themeManager->backgroundColor().name(),          // disabled bg
                                   m_themeManager->subtitleColor().name(),            // disabled text
                                   m_themeManager->subtitleColor().name());           // placeholder

  // Apply to Sign In tab fields
  m_loginUsernameEdit->setStyleSheet(lineEditStyle);
  m_loginPasswordEdit->setStyleSheet(lineEditStyle);
  // Apply to Register tab fields
  m_usernameEdit->setStyleSheet(lineEditStyle);
  m_emailEdit->setStyleSheet(lineEditStyle);
  m_passwordEdit->setStyleSheet(lineEditStyle);

  // Enhanced button styling with proper contrast
  QString whiteText = QColor(Qt::white).name();
  QString buttonStyle =
      QString(R"(
        QPushButton {
            background-color: %1;
            color: %6;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            padding: 0 18px;
            min-height: 44px;
        }
        QPushButton:hover {
            background-color: %2;
            color: %6;
        }
        QPushButton:pressed {
            background-color: %3;
            color: %6;
        }
        QPushButton:disabled {
            background-color: %4;
            color: %5;
        }
    )")
          .arg(accentHex,
               m_themeManager->accentColor().lighter(110).name(),
               m_themeManager->accentColor().darker(110).name(),
               m_themeManager->secondaryColor().name(),     // disabled bg
               m_themeManager->subtitleColor().name(),      // disabled text
               whiteText);                                   // button text color

  m_loginButton->setStyleSheet(buttonStyle);
  m_registerButton->setStyleSheet(buttonStyle);

  // Secondary buttons (Reveal/Restore) - outlined style with proper contrast
  QString secondaryButtonStyle =
      QString(R"(
        QPushButton {
            background-color: transparent;
            color: %1;
            border: 2px solid %1;
            border-radius: 8px;
            font-size: 13px;
            font-weight: 500;
            padding: 0 16px;
            min-height: 36px;
        }
        QPushButton:hover {
            background-color: %2;
            border-color: %3;
            color: %1;
        }
        QPushButton:pressed {
            background-color: %4;
            color: %1;
        }
    )")
          .arg(accentHex,
               m_themeManager->accentColor().lighter(180).name(),  // hover bg (very light)
               m_themeManager->accentColor().lighter(120).name(),  // hover border
               m_themeManager->accentColor().lighter(160).name()); // pressed bg

  m_revealSeedButton->setStyleSheet(secondaryButtonStyle);
  m_restoreSeedButton->setStyleSheet(secondaryButtonStyle);

  // Password toggle buttons with proper hover text color transition
  QString toggleButtonStyle =
      QString(R"(
        QPushButton {
            font-size: 12px;
            border: none;
            border-radius: 4px;
            background-color: transparent;
            color: %1;
            font-weight: 500;
            padding: 4px 8px;
        }
        QPushButton:hover {
            color: %2;
            background-color: %3;
        }
        QPushButton:pressed {
            color: %2;
            background-color: %4;
        }
    )")
          .arg(textHex, accentHex,
               m_themeManager->secondaryColor().name(),        // hover bg
               m_themeManager->secondaryColor().darker(110).name()); // pressed bg

  m_loginPasswordToggleButton->setStyleSheet(toggleButtonStyle);
  m_passwordToggleButton->setStyleSheet(toggleButtonStyle);

  // Position the toggle buttons inside their respective password fields
  QTimer::singleShot(0, this, [this]() {
    // Position login password toggle button
    if (m_loginPasswordEdit && m_loginPasswordToggleButton) {
      const int buttonWidth = m_loginPasswordToggleButton->width();
      const int padding = 8;
      m_loginPasswordEdit->setTextMargins(0, 0, buttonWidth + padding, 0);
      const int x = m_loginPasswordEdit->width() - buttonWidth - padding;
      const int y = (m_loginPasswordEdit->height() -
                     m_loginPasswordToggleButton->height()) /
                    2;
      m_loginPasswordToggleButton->move(x, y);
    }
    // Position register password toggle button
    if (m_passwordEdit && m_passwordToggleButton) {
      const int buttonWidth = m_passwordToggleButton->width();
      const int padding = 8;
      m_passwordEdit->setTextMargins(0, 0, buttonWidth + padding, 0);
      const int x = m_passwordEdit->width() - buttonWidth - padding;
      const int y =
          (m_passwordEdit->height() - m_passwordToggleButton->height()) / 2;
      m_passwordToggleButton->move(x, y);
    }
  });

  // Apply fonts
  QFont titleF = m_themeManager->titleFont();
  titleF.setPointSizeF(titleF.pointSizeF() + 6);
  m_titleLabel->setFont(titleF);
  QFont subtitleF = m_themeManager->textFont();
  subtitleF.setPointSizeF(subtitleF.pointSizeF() + 2);
  m_subtitleLabel->setFont(subtitleF);
  m_loginButton->setFont(m_themeManager->buttonFont());
  m_registerButton->setFont(m_themeManager->buttonFont());
  m_revealSeedButton->setFont(m_themeManager->buttonFont());
  m_restoreSeedButton->setFont(m_themeManager->buttonFont());
  // Apply fonts to Sign In tab fields
  m_loginUsernameEdit->setFont(m_themeManager->textFont());
  m_loginPasswordEdit->setFont(m_themeManager->textFont());
  // Apply fonts to Register tab fields
  m_usernameEdit->setFont(m_themeManager->textFont());
  m_emailEdit->setFont(m_themeManager->textFont());
  m_passwordEdit->setFont(m_themeManager->textFont());
}

void QtLoginUI::showMessage(const QString &message, bool isError) {
  if (m_messageLabel) {
    m_messageLabel->setText(message);
    m_messageLabel->setProperty("isError", isError);
    m_messageLabel->setStyleSheet(
        isError ? m_themeManager->getErrorMessageStyleSheet()
                : m_themeManager->getSuccessMessageStyleSheet());
    m_messageLabel->show();
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

void QtLoginUI::validateRegisterForm() {
  // Enable Register button only if all fields are filled
  const bool allFieldsFilled = !m_usernameEdit->text().trimmed().isEmpty() &&
                               !m_emailEdit->text().trimmed().isEmpty() &&
                               !m_passwordEdit->text().isEmpty();
  m_registerButton->setEnabled(allFieldsFilled);
}

bool QtLoginUI::eventFilter(QObject *watched, QEvent *event) {
  if (event->type() == QEvent::Resize) {
    // Reposition password toggle buttons on resize
    if (watched == m_loginPasswordEdit && m_loginPasswordToggleButton) {
      QTimer::singleShot(0, this, [this]() {
        if (!m_loginPasswordToggleButton || !m_loginPasswordEdit)
          return;
        const int buttonWidth = m_loginPasswordToggleButton->width();
        const int padding = 8;
        m_loginPasswordEdit->setTextMargins(0, 0, buttonWidth + padding, 0);
        const int x = m_loginPasswordEdit->width() - buttonWidth - padding;
        const int y = (m_loginPasswordEdit->height() -
                       m_loginPasswordToggleButton->height()) /
                      2;
        m_loginPasswordToggleButton->move(x, y);
      });
    } else if (watched == m_passwordEdit && m_passwordToggleButton) {
      QTimer::singleShot(0, this, [this]() {
        if (!m_passwordToggleButton || !m_passwordEdit)
          return;
        const int buttonWidth = m_passwordToggleButton->width();
        const int padding = 8;
        m_passwordEdit->setTextMargins(0, 0, buttonWidth + padding, 0);
        const int x = m_passwordEdit->width() - buttonWidth - padding;
        const int y =
            (m_passwordEdit->height() - m_passwordToggleButton->height()) / 2;
        m_passwordToggleButton->move(x, y);
      });
    }
  }
  return QWidget::eventFilter(watched, event);
}
