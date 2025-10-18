// QtLoginUI.cpp
#include "QtLoginUI.h"
#include "Auth.h"
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
  m_mainLayout->setContentsMargins(40, 40, 40, 40);
  m_mainLayout->setSpacing(16);

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

    m_subtitleLabel = new QLabel("Secure Bitcoin Wallet", header);
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

  setupThemeSelector();
}

void QtLoginUI::createLoginCard() {
  m_loginCard = new QFrame(this);
  m_loginCard->setProperty("class", "card");
  m_loginCard->setFixedSize(480, 460);

  m_cardLayout = new QVBoxLayout(m_loginCard);
  m_cardLayout->setContentsMargins(24, 24, 24, 16);
  m_cardLayout->setSpacing(12);

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
  tabBarLayout->setContentsMargins(0, 0, 0, 0);
  tabBarLayout->setSpacing(0);
  tabBarLayout->addStretch();
  tabBarLayout->addWidget(m_tabBar);
  tabBarLayout->addStretch();

  m_cardLayout->addWidget(tabBarContainer);

  // Stacked widget for tab content
  m_stackedWidget = new QStackedWidget(m_loginCard);
  m_cardLayout->addWidget(m_stackedWidget);

  // Connect tab bar to stacked widget
  connect(m_tabBar, &QTabBar::currentChanged, m_stackedWidget, &QStackedWidget::setCurrentIndex);

  // ===== Sign In Tab =====
  QWidget *signInTab = new QWidget();
  QVBoxLayout *signInLayout = new QVBoxLayout(signInTab);
  signInLayout->setContentsMargins(32, 24, 32, 24);
  signInLayout->setSpacing(12);
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
  registerLayout->setContentsMargins(32, 24, 32, 24);
  registerLayout->setSpacing(12);
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

  // Message label (shared between tabs)
  m_messageLabel = new QLabel(m_loginCard);
  m_messageLabel->setAlignment(Qt::AlignCenter);
  m_messageLabel->setWordWrap(true);
  m_messageLabel->setMinimumHeight(30);
  m_messageLabel->setStyleSheet(m_themeManager->getMessageStyleSheet());
  m_messageLabel->hide();
  m_cardLayout->addWidget(m_messageLabel);

  m_cardLayout->addSpacing(4);

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

void QtLoginUI::setupThemeSelector() {
  m_themeLayout = new QHBoxLayout();
  m_themeLayout->setSpacing(8);
  m_themeLayout->addItem(
      new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum));

  QLabel *themeLabel = new QLabel("Theme:");
  themeLabel->setObjectName("themeLabel");
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
  showMessage(message, !success);
  m_loginPasswordEdit->clear();
}

void QtLoginUI::onRegisterResult(bool success, const QString &message) {
  showMessage(message, !success);

  if (success) {
    const QString username = m_usernameEdit->text().trimmed();
    m_usernameEdit->clear();
    m_emailEdit->clear();
    m_passwordEdit->clear();

    // Enhanced post-registration seed backup modal
    QDialog dlg(this);
    dlg.setWindowTitle("Wallet Seed Phrase Backup");
    dlg.setModal(true);
    dlg.setMinimumSize(500, 400);
    dlg.resize(500, 400); // Set initial size to match minimum size

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);

    // Header section
    QLabel *title = new QLabel("<h2 style='color: #2E7D32; margin: 0;'>Account "
                               "Created Successfully</h2>"
                               "<p style='margin: 8px 0;'>Your 12-word seed "
                               "phrase has been generated.</p>",
                               &dlg);
    title->setAlignment(Qt::AlignCenter);
    title->setWordWrap(true);

    // Warning section
    QFrame *warningFrame = new QFrame(&dlg);
    warningFrame->setFrameStyle(QFrame::Box);

    QVBoxLayout *warningLayout = new QVBoxLayout(warningFrame);
    warningLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *warningTitle =
        new QLabel("<b>IMPORTANT: Backup Your Seed Phrase</b>", &dlg);

    QLabel *warningText = new QLabel(
        "• Write down these 12 words on paper and store securely<br/>"
        "• This is the only way to recover your wallet<br/>"
        "• Never share or store digitally",
        &dlg);
    warningText->setWordWrap(true);

    warningLayout->addWidget(warningTitle);
    warningLayout->addWidget(warningText);

    // Backup file section
    QFrame *backupFrame = new QFrame(&dlg);
    backupFrame->setFrameStyle(QFrame::Box);

    QVBoxLayout *backupLayout = new QVBoxLayout(backupFrame);
    backupLayout->setContentsMargins(12, 12, 12, 12);

    QLabel *backupTitle = new QLabel("📁 <b>Your Seed Phrase File</b>", &dlg);

    QLabel *backupText = new QLabel(
        "A temporary backup file has been created with your seed phrase.<br/>"
        "<b>Important:</b> This file will be deleted after you confirm backup.",
        &dlg);
    backupText->setWordWrap(true);

    QPushButton *openFolder = new QPushButton("Open Backup Folder", &dlg);

    backupLayout->addWidget(backupTitle);
    backupLayout->addWidget(backupText);
    backupLayout->addWidget(openFolder);

    // Confirmation section
    QFrame *confirmFrame = new QFrame(&dlg);
    confirmFrame->setFrameStyle(QFrame::Box);

    QVBoxLayout *confirmLayout = new QVBoxLayout(confirmFrame);
    confirmLayout->setContentsMargins(12, 12, 12, 12);

    QCheckBox *confirm1 =
        new QCheckBox("I have written down all 12 words on paper", &dlg);
    QCheckBox *confirm2 = new QCheckBox(
        "I understand this is the only way to recover my wallet", &dlg);
    QCheckBox *confirm3 = new QCheckBox(
        "I will delete the backup file after confirming my backup", &dlg);

    confirmLayout->addWidget(confirm1);
    confirmLayout->addWidget(confirm2);
    confirmLayout->addWidget(confirm3);

    // Buttons
    QDialogButtonBox *box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dlg);
    box->setCenterButtons(true);
    QPushButton *okBtn = box->button(QDialogButtonBox::Ok);
    okBtn->setText("Continue to Wallet");
    okBtn->setEnabled(false);

    QPushButton *cancelBtn = box->button(QDialogButtonBox::Cancel);
    cancelBtn->setText("Cancel");

    // Enable OK button only when all checkboxes are checked
    auto updateOkButton = [=]() {
      bool allChecked = confirm1->isChecked() && confirm2->isChecked() &&
                        confirm3->isChecked();
      okBtn->setEnabled(allChecked);
    };

    connect(confirm1, &QCheckBox::toggled, updateOkButton);
    connect(confirm2, &QCheckBox::toggled, updateOkButton);
    connect(confirm3, &QCheckBox::toggled, updateOkButton);

    connect(openFolder, &QPushButton::clicked, [this, username]() {
      try {
        // Get the current working directory
        QString currentDir = QDir::currentPath();

        // Create seed_vault directory if it doesn't exist
        QString seedDir = currentDir + "/seed_vault";
        QDir mainDir(seedDir);
        if (!mainDir.exists()) {
          mainDir.mkpath(seedDir);
        }

        // Check for user-specific backup files
        QString userDir = seedDir + "/" + username;
        QDir dir(userDir);
        QString userBackupFile = userDir + "/SEED_BACKUP_12_WORDS.txt";
        QString fallbackFile =
            seedDir + "/" + username + "_mnemonic_SHOW_ONCE.txt";

        bool success = false;
        if (dir.exists() && QFile::exists(userBackupFile)) {
          // User-specific directory exists and has backup file
          success = QDesktopServices::openUrl(QUrl::fromLocalFile(userDir));
          if (!success) {
            QMessageBox::warning(
                this, "Error",
                "Failed to open user-specific backup folder: " + userDir);
          }
        } else if (QFile::exists(fallbackFile)) {
          // Fallback file exists in main directory, select it
          success = QDesktopServices::openUrl(QUrl::fromLocalFile(seedDir));
          if (!success) {
            QMessageBox::warning(this, "Error",
                                 "Failed to open backup folder: " + seedDir);
          }
        } else {
          // No backup files found - open main directory as last resort
          success = QDesktopServices::openUrl(QUrl::fromLocalFile(seedDir));
          if (!success) {
            QMessageBox::warning(this, "Error",
                                 "Failed to open backup folder: " + seedDir);
          }
          QMessageBox::information(
              this, "Note",
              "No backup file found. This may be because:\n"
              "1. Account creation failed to generate backup\n"
              "2. Backup file was already deleted\n"
              "3. File permissions issue\n\n"
              "If you just created an account, try the 'Reveal Seed' button "
              "instead.");
        }
      } catch (const std::exception &e) {
        QMessageBox::critical(
            this, "Error",
            QString("Exception while opening folder: %1").arg(e.what()));
      } catch (...) {
        QMessageBox::critical(this, "Error",
                              "Unknown error occurred while opening folder.");
      }
    });
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    // Add all widgets to main layout
    mainLayout->addWidget(title);
    mainLayout->addWidget(warningFrame);
    mainLayout->addWidget(backupFrame);
    mainLayout->addWidget(confirmFrame);
    mainLayout->addWidget(box);

    // Clear input fields regardless of dialog result
    m_usernameEdit->clear();
    m_emailEdit->clear();
    m_passwordEdit->clear();
    clearMessage();

    // Switch to Sign In tab so user can log in with their new account
    m_tabBar->setCurrentIndex(0);
  } else {
    m_emailEdit->clear();
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
  QString rootCss = m_themeManager->getMainWindowStyleSheet();

  QColor win = palette().color(QPalette::Window);
  QString winHex = QString("#%1%2%3")
                       .arg(win.red(), 2, 16, QLatin1Char('0'))
                       .arg(win.green(), 2, 16, QLatin1Char('0'))
                       .arg(win.blue(), 2, 16, QLatin1Char('0'));

  QColor baseText = palette().color(QPalette::WindowText);
  QColor base = palette().color(QPalette::Base);
  QString baseHex = QString("#%1%2%3")
                        .arg(base.red(), 2, 16, QLatin1Char('0'))
                        .arg(base.green(), 2, 16, QLatin1Char('0'))
                        .arg(base.blue(), 2, 16, QLatin1Char('0'));

  QColor subtitleColor =
      (win.value() < 128) ? baseText.lighter(160) : baseText.darker(160);
  QString subtitleHex = QString("#%1%2%3")
                            .arg(subtitleColor.red(), 2, 16, QLatin1Char('0'))
                            .arg(subtitleColor.green(), 2, 16, QLatin1Char('0'))
                            .arg(subtitleColor.blue(), 2, 16, QLatin1Char('0'));

  QString textHex = m_themeManager->textColor().name();
  QString accentHex = m_themeManager->accentColor().name();
  QString cardBg = m_themeManager->backgroundColor().name();
  QString borderColor = (win.value() < 128) ? "#404040" : "#d0d0d0";

  rootCss += QString(R"(
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
        QLabel#themeLabel {
            background-color: %1 !important;
            border: none !important;
            color: %3;
        }
    )")
                 .arg(winHex, subtitleHex, textHex);

  setStyleSheet(rootCss);

  // Enhanced card styling with proper background
  const QString cardCss = QString(R"(
        QFrame[class="card"] {
            background-color: %1;
            border: 2px solid %2;
            border-radius: 12px;
        }
    )")
                              .arg(cardBg, borderColor);
  m_loginCard->setStyleSheet(cardCss);

  // Tab Bar Styling - Subtle and modern design (centering handled by layout)
  QString tabBarStyle =
      QString(R"(
        QTabBar {
            background: transparent;
            border: none;
        }
        QTabBar::tab {
            background: transparent;
            color: %2;
            padding: 12px 24px;
            margin-left: 4px;
            margin-right: 4px;
            margin-bottom: 2px;
            margin-top: 0px;
            border: none;
            border-bottom: 2px solid transparent;
            font-size: 14px;
            font-weight: 500;
        }
        QTabBar::tab:selected {
            background: transparent;
            color: %4;
            border-bottom: 2px solid %4;
            font-weight: 600;
        }
        QTabBar::tab:hover:!selected {
            color: %4;
            background: transparent;
        }
    )")
          .arg(baseHex, textHex, borderColor, accentHex, cardBg,
               (win.value() < 128) ? "#2a2a2a" : "#f0f0f0");
  m_tabBar->setStyleSheet(tabBarStyle);

  // Enhanced LineEdit styling with proper backgrounds
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
                              .arg(baseHex, textHex, borderColor, accentHex,
                                   (win.value() < 128) ? "#505050" : "#b0b0b0",
                                   (win.value() < 128) ? "#1a1a1a" : "#e8e8e8",
                                   (win.value() < 128) ? "#808080" : "#909090");

  // Apply to Sign In tab fields
  m_loginUsernameEdit->setStyleSheet(lineEditStyle);
  m_loginPasswordEdit->setStyleSheet(lineEditStyle);
  // Apply to Register tab fields
  m_usernameEdit->setStyleSheet(lineEditStyle);
  m_emailEdit->setStyleSheet(lineEditStyle);
  m_passwordEdit->setStyleSheet(lineEditStyle);

  // Enhanced button styling
  QString buttonStyle =
      QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
            padding: 0 18px;
            min-height: 44px;
        }
        QPushButton:hover {
            background-color: %3;
        }
        QPushButton:pressed {
            background-color: %4;
        }
        QPushButton:disabled {
            background-color: %5;
            color: %6;
        }
    )")
          .arg(accentHex, cardBg,
               m_themeManager->accentColor().lighter(110).name(),
               m_themeManager->accentColor().darker(110).name(),
               (win.value() < 128) ? "#2a2a2a" : "#d0d0d0",
               (win.value() < 128) ? "#505050" : "#a0a0a0");

  m_loginButton->setStyleSheet(buttonStyle);
  m_registerButton->setStyleSheet(buttonStyle);

  // Secondary buttons (Reveal/Restore) - outlined style
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
        }
        QPushButton:pressed {
            background-color: %4;
        }
    )")
          .arg(accentHex, (win.value() < 128) ? "#1a2a3a" : "#e8f0ff",
               m_themeManager->accentColor().lighter(120).name(),
               (win.value() < 128) ? "#0a1a2a" : "#d0e0ff");

  m_revealSeedButton->setStyleSheet(secondaryButtonStyle);
  m_restoreSeedButton->setStyleSheet(secondaryButtonStyle);

  // Password toggle buttons
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
    )")
          .arg(textHex, accentHex, (win.value() < 128) ? "#2a2a2a" : "#f0f0f0");

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
