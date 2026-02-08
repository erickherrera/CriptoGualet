#include "QtSettingsUI.h"
#include "../../backend/core/include/Auth.h"
#include "../../backend/repository/include/Repository/SettingsRepository.h"
#include "../../backend/repository/include/Repository/UserRepository.h"
#include "../../backend/repository/include/Repository/WalletRepository.h"
#include "../../backend/utils/include/QRGenerator.h"
#include "QtPasswordConfirmDialog.h"
#include "QtThemeManager.h"
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <QFutureWatcher>
#include "../../backend/blockchain/include/BitcoinProvider.h"
#include <map>
#include <optional>
#include <vector>

extern std::string g_currentUser;

namespace {
constexpr const char *kSettingsProviderTypeKey = "btc.provider";
constexpr const char *kSettingsRpcUrlKey = "btc.rpc.url";
constexpr const char *kSettingsRpcUsernameKey = "btc.rpc.username";
constexpr const char *kSettingsRpcPasswordKey = "btc.rpc.password";
constexpr const char *kSettingsRpcAllowInsecureKey = "btc.rpc.allow_insecure";
constexpr const char *kSettingsProviderFallbackKey = "btc.provider.fallback";
} // namespace

QtSettingsUI::QtSettingsUI(QWidget *parent)
    : QWidget(parent), m_themeManager(&QtThemeManager::instance()),
      m_mainLayout(nullptr), m_centeringLayout(nullptr),
      m_leftSpacer(nullptr), m_rightSpacer(nullptr),
      m_scrollArea(nullptr), m_centerContainer(nullptr),
      m_2FATitleLabel(nullptr), m_2FAStatusLabel(nullptr),
      m_enable2FAButton(nullptr), m_disable2FAButton(nullptr),
      m_2FADescriptionLabel(nullptr), m_userRepository(nullptr),
      m_walletRepository(nullptr), m_settingsRepository(nullptr),
      m_currentUserId(-1), m_btcProviderSelector(nullptr),
      m_btcRpcUrlEdit(nullptr), m_btcRpcUsernameEdit(nullptr),
      m_btcRpcPasswordEdit(nullptr), m_btcAllowInsecureCheck(nullptr),
      m_btcEnableFallbackCheck(nullptr), m_btcProviderStatusLabel(nullptr),
      m_btcTestConnectionButton(nullptr), m_btcSaveSettingsButton(nullptr),
      m_hardwareWalletSelector(nullptr), m_hardwareDerivationPathEdit(nullptr),
      m_hardwareUseTestnetCheck(nullptr), m_hardwareDetectButton(nullptr),
      m_hardwareImportXpubButton(nullptr), m_hardwareStatusLabel(nullptr),
      m_hardwareXpubDisplay(nullptr) {
  setupUI();
  applyTheme();

  // Connect to theme changes
  connect(m_themeManager, &QtThemeManager::themeChanged, this,
          &QtSettingsUI::applyTheme);
}

void QtSettingsUI::setRepositories(
    Repository::UserRepository *userRepository,
    Repository::WalletRepository *walletRepository,
    Repository::SettingsRepository *settingsRepository) {
  m_userRepository = userRepository;
  m_walletRepository = walletRepository;
  m_settingsRepository = settingsRepository;
  loadAdvancedSettings();
}

void QtSettingsUI::setCurrentUserId(int userId) {
  m_currentUserId = userId;
  loadAdvancedSettings();
}

void QtSettingsUI::setupUI() {
  // Main layout - maximize width for laptop screens
  QVBoxLayout *outerLayout = new QVBoxLayout(this);
  outerLayout->setContentsMargins(0, 0, 0, 0);
  outerLayout->setSpacing(0);

  // Use a Scroll Area for laptop screens
  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  // Don't set stylesheet here - will be set in updateStyles()

  // Container that fills the width
  m_centerContainer = new QWidget();
  m_centerContainer->setObjectName("centerContainer");
  // Removed setMaximumWidth(900) to maximize space usage
  // Don't set stylesheet here - will be set in updateStyles()

  m_mainLayout = new QVBoxLayout(m_centerContainer);
  // Optimized for laptop screens (reduced margins/spacing)
  // Increased top margin to account for floating Sign Out button
  m_mainLayout->setContentsMargins(
      m_themeManager->spacing(6), // 24px
      m_themeManager->spacing(8), // 32px - increased for floating button
      m_themeManager->spacing(6), // 24px
      m_themeManager->spacing(5)  // 20px
  );
  m_mainLayout->setSpacing(m_themeManager->spacing(4)); // 16px

  m_scrollArea->setWidget(m_centerContainer);

  // Create a horizontal layout to center content with max width
  m_centeringLayout = new QHBoxLayout();

  // Add left spacer (expanding)
  m_leftSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_centeringLayout->addItem(m_leftSpacer);

  // Add scroll area directly to centering layout
  m_centeringLayout->addWidget(m_scrollArea);

  // Add right spacer (expanding)
  m_rightSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_centeringLayout->addItem(m_rightSpacer);

  // Add centering layout to outer layout
  outerLayout->addLayout(m_centeringLayout);

  // Initialize responsive layout
  QTimer::singleShot(0, this, [this]() { updateScrollAreaWidth(); });

  // Title
  m_titleLabel = new QLabel("Settings", m_centerContainer);
  m_titleLabel->setProperty("class", "title");
  m_titleLabel->setAlignment(Qt::AlignLeft);
  m_mainLayout->addWidget(m_titleLabel);

  // Theme Settings Group
  QGroupBox *themeGroup = new QGroupBox("Appearance", m_centerContainer);
  QFormLayout *themeLayout = new QFormLayout(themeGroup);
  themeLayout->setContentsMargins(15, 15, 15, 15);
  themeLayout->setSpacing(10);

  // Theme selector
  QLabel *themeLabel = new QLabel("Theme:", m_centerContainer);
  themeLabel->setStyleSheet("QLabel { background-color: transparent; }");

  m_themeSelector = new QComboBox(m_centerContainer);
  m_themeSelector->addItem("Dark - Blue", static_cast<int>(ThemeType::DARK));
  m_themeSelector->addItem("Light - Blue", static_cast<int>(ThemeType::LIGHT));
  m_themeSelector->addItem("Dark - Purple",
                           static_cast<int>(ThemeType::CRYPTO_DARK));
  m_themeSelector->addItem("Light - Purple",
                           static_cast<int>(ThemeType::CRYPTO_LIGHT));
  ThemeType currentTheme = m_themeManager->getCurrentTheme();
  m_themeSelector->setCurrentIndex(
      m_themeSelector->findData(static_cast<int>(currentTheme)));

  connect(m_themeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this](int index) {
            int themeValue = m_themeSelector->itemData(index).toInt();
            m_themeManager->applyTheme(static_cast<ThemeType>(themeValue));
          });

  themeLayout->addRow(themeLabel, m_themeSelector);

  m_mainLayout->addWidget(themeGroup);

  // Security Settings Group - 2FA
  QGroupBox *securityGroup = new QGroupBox("Security", m_centerContainer);
  securityGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  securityGroup->setVisible(true);
  QVBoxLayout *securityLayout = new QVBoxLayout(securityGroup);
  securityLayout->setContentsMargins(15, 20, 15,
                                     15); // Compacted (was 20, 25, 20, 20)
  securityLayout->setSpacing(8);          // Compacted (was 12)
  // Apply group box styling immediately for visibility
  QString groupBoxStyle = QString(R"(
        QGroupBox {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 20px;
            font-size: 16px;
            font-weight: 600;
            color: %3;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 15px;
            padding: 0 5px;
            margin-top: 5px;
        }
    )")
                              .arg(m_themeManager->surfaceColor().name())
                              .arg(m_themeManager->secondaryColor().name())
                              .arg(m_themeManager->textColor().name());
  securityGroup->setStyleSheet(groupBoxStyle);

  // 2FA Title
  m_2FATitleLabel =
      new QLabel("Two-Factor Authentication (2FA)", securityGroup);
  m_2FATitleLabel->setProperty("class", "title");
  m_2FATitleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_2FATitleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  m_2FATitleLabel->setWordWrap(false);
  securityLayout->addWidget(m_2FATitleLabel);

  // 2FA Description
  m_2FADescriptionLabel = new QLabel(
      "Two-factor authentication adds an extra layer of security by requiring "
      "a code from your authenticator app when signing in. Compatible with "
      "Google Authenticator, Authy, Microsoft Authenticator, and other TOTP "
      "apps.",
      securityGroup);
  m_2FADescriptionLabel->setProperty("class", "subtitle");
  m_2FADescriptionLabel->setSizePolicy(QSizePolicy::Expanding,
                                       QSizePolicy::Minimum);
  m_2FADescriptionLabel->setWordWrap(true);
  m_2FADescriptionLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  securityLayout->addWidget(m_2FADescriptionLabel);

  // 2FA Status Label
  m_2FAStatusLabel = new QLabel("Loading...", securityGroup);
  m_2FAStatusLabel->setProperty("class", "subtitle");
  m_2FAStatusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_2FAStatusLabel->setWordWrap(true);
  m_2FAStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  securityLayout->addWidget(m_2FAStatusLabel);

  // Button container
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(10);

  // Enable 2FA Button (shown when 2FA is disabled)
  m_enable2FAButton = new QPushButton("Enable 2FA", securityGroup);
  m_enable2FAButton->setProperty("class", "accent-button");
  m_enable2FAButton->setMinimumWidth(120);
  m_enable2FAButton->setMaximumWidth(150);
  m_enable2FAButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  m_enable2FAButton->setEnabled(true);
  buttonLayout->addWidget(m_enable2FAButton);
  m_enable2FAButton->hide(); // Initially hidden, will be shown by
                             // update2FAStatus() if needed

  // Disable 2FA Button (shown when 2FA is enabled)
  m_disable2FAButton = new QPushButton("Disable 2FA", securityGroup);
  m_disable2FAButton->setProperty("class", "secondary-button");
  m_disable2FAButton->setMinimumWidth(120);
  m_disable2FAButton->setMaximumWidth(150);
  m_disable2FAButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  m_disable2FAButton->setEnabled(true);
  buttonLayout->addWidget(m_disable2FAButton);
  m_disable2FAButton->hide(); // Initially hidden, will be shown by
                              // update2FAStatus() if needed

  buttonLayout->addStretch();
  securityLayout->addLayout(buttonLayout);

  // Connect signals for 2FA enablement
  connect(m_enable2FAButton, &QPushButton::clicked, this,
          &QtSettingsUI::onEnable2FAClicked);
  connect(m_disable2FAButton, &QPushButton::clicked, this,
          &QtSettingsUI::onDisable2FAClicked);

  // Update 2FA status to show/hide appropriate buttons
  update2FAStatus();

  m_mainLayout->addWidget(securityGroup);

  QGroupBox *walletGroup = new QGroupBox("Advanced wallet settings", m_centerContainer);
  QVBoxLayout *walletLayout = new QVBoxLayout(walletGroup);
  walletLayout->setContentsMargins(15, 15, 15, 15); // Compacted (was 20)
  walletLayout->setSpacing(12);

  m_walletPlaceholder = new QLabel(
      "Configure node providers and hardware wallets for this device.",
      walletGroup);
  m_walletPlaceholder->setProperty("class", "subtitle");
  m_walletPlaceholder->setWordWrap(true);
  QFont italicFont = m_themeManager->textFont();
  italicFont.setItalic(true);
  m_walletPlaceholder->setFont(italicFont);
  walletLayout->addWidget(m_walletPlaceholder);

  // --- Bitcoin Provider Section (Refactored to Floating Dialog) ---
  QFrame* providerSection = new QFrame(walletGroup);
  QVBoxLayout* providerSectionLayout = new QVBoxLayout(providerSection);
  providerSectionLayout->setContentsMargins(0, 10, 0, 10);
  providerSectionLayout->setSpacing(8);

  QLabel* providerHeader = new QLabel("Bitcoin Node Provider", providerSection);
  providerHeader->setStyleSheet(QString("font-weight: 600; font-size: 16px; color: %1;")
      .arg(m_themeManager->textColor().name()));
  
  m_btcProviderStatusLabel = new QLabel("Loading provider settings...", providerSection);
  m_btcProviderStatusLabel->setProperty("class", "subtitle");
  m_btcProviderStatusLabel->setWordWrap(true);

  QPushButton* configureProviderButton = new QPushButton("Configure Node Provider", providerSection);
  configureProviderButton->setObjectName("configureProviderButton");
  configureProviderButton->setCursor(Qt::PointingHandCursor);
  configureProviderButton->setMinimumWidth(200);
  configureProviderButton->setStyleSheet(m_themeManager->getOutlinedButtonStyleSheet());

  providerSectionLayout->addWidget(providerHeader);
  providerSectionLayout->addWidget(m_btcProviderStatusLabel);
  providerSectionLayout->addWidget(configureProviderButton, 0, Qt::AlignLeft);

  walletLayout->addWidget(providerSection);

  // Connect configuration button to floating dialog logic
  connect(configureProviderButton, &QPushButton::clicked, this, [this]() {
      if (!m_settingsRepository || m_currentUserId <= 0) {
          QMessageBox::warning(this, "Not Signed In", "Please sign in to configure settings.");
          return;
      }

      // Create floating dialog
      QDialog* dialog = new QDialog(this);
      dialog->setWindowTitle("Configure Bitcoin Node");
      dialog->setModal(true);
      
      // Fullscreen/Parent overlay sizing
      if (auto* screen = QGuiApplication::primaryScreen()) {
          dialog->resize(screen->size());
      } else {
          dialog->resize(this->size());
      }
      
      dialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
      dialog->setAttribute(Qt::WA_TranslucentBackground);
      dialog->setStyleSheet("QDialog { background-color: rgba(0, 0, 0, 150); }");

      // Main card layout
      QVBoxLayout* dialogLayout = new QVBoxLayout(dialog);
      dialogLayout->setContentsMargins(0, 0, 0, 0);
      dialogLayout->setAlignment(Qt::AlignCenter);

      // Card Frame
      QFrame* card = new QFrame(dialog);
      card->setObjectName("rpcCard");
      card->setMinimumWidth(500);
      card->setMaximumWidth(600);
      
      QString cardStyle = QString(R"(
          QFrame#rpcCard {
              background-color: %1;
              border-radius: 16px;
              border: 1px solid %2;
          }
      )")
      .arg(m_themeManager->surfaceColor().name())
      .arg(m_themeManager->secondaryColor().name());
      card->setStyleSheet(cardStyle);

      // Drop shadow
      QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(card);
      shadow->setBlurRadius(30);
      shadow->setColor(QColor(0, 0, 0, 80));
      shadow->setOffset(0, 8);
      card->setGraphicsEffect(shadow);

      QVBoxLayout* cardLayout = new QVBoxLayout(card);
      cardLayout->setContentsMargins(30, 30, 30, 30);
      cardLayout->setSpacing(20);

      // Header
      QLabel* titleLabel = new QLabel("Bitcoin Node Configuration", card);
      titleLabel->setStyleSheet(QString("font-size: 22px; font-weight: 700; color: %1;").arg(m_themeManager->textColor().name()));
      cardLayout->addWidget(titleLabel);

      QLabel* descLabel = new QLabel("Connect to BlockCypher's public API or your own Bitcoin Core node.", card);
      descLabel->setWordWrap(true);
      descLabel->setStyleSheet(QString("font-size: 14px; color: %1;").arg(m_themeManager->subtitleColor().name()));
      cardLayout->addWidget(descLabel);

      // Form Content
      QComboBox* providerSelector = new QComboBox(card);
      providerSelector->addItem("BlockCypher (default)", "blockcypher");
      providerSelector->addItem("Bitcoin Core RPC", "rpc");
      
      QFrame* rpcFrame = new QFrame(card);
      QVBoxLayout* rpcLayout = new QVBoxLayout(rpcFrame);
      rpcLayout->setContentsMargins(0, 0, 0, 0);
      rpcLayout->setSpacing(12);

      QLineEdit* urlEdit = new QLineEdit(rpcFrame);
      urlEdit->setPlaceholderText("RPC URL (e.g., http://127.0.0.1:8332)");
      
      QLineEdit* userEdit = new QLineEdit(rpcFrame);
      userEdit->setPlaceholderText("RPC Username");
      
      QLineEdit* passEdit = new QLineEdit(rpcFrame);
      passEdit->setPlaceholderText("RPC Password");
      passEdit->setEchoMode(QLineEdit::Password);

      QCheckBox* insecureCheck = new QCheckBox("Allow HTTP (Insecure)", rpcFrame);
      QCheckBox* fallbackCheck = new QCheckBox("Fallback to BlockCypher on failure", rpcFrame);

      rpcLayout->addWidget(new QLabel("Node Details:", rpcFrame));
      rpcLayout->addWidget(urlEdit);
      rpcLayout->addWidget(userEdit);
      rpcLayout->addWidget(passEdit);
      rpcLayout->addWidget(insecureCheck);
      rpcLayout->addWidget(fallbackCheck);

      cardLayout->addWidget(new QLabel("Provider Type:", card));
      cardLayout->addWidget(providerSelector);
      cardLayout->addWidget(rpcFrame);

      // Load current settings
      std::vector<std::string> keys = {kSettingsProviderTypeKey, kSettingsRpcUrlKey,
                                       kSettingsRpcUsernameKey, kSettingsRpcPasswordKey,
                                       kSettingsRpcAllowInsecureKey, kSettingsProviderFallbackKey};
      auto settings = m_settingsRepository->getUserSettings(m_currentUserId, keys).data;
      
      QString currentProvider = QString::fromStdString(settings[kSettingsProviderTypeKey]);
      if (currentProvider.isEmpty()) currentProvider = "blockcypher";
      providerSelector->setCurrentIndex(providerSelector->findData(currentProvider));
      
      urlEdit->setText(QString::fromStdString(settings[kSettingsRpcUrlKey]));
      userEdit->setText(QString::fromStdString(settings[kSettingsRpcUsernameKey]));
      passEdit->setText(QString::fromStdString(settings[kSettingsRpcPasswordKey]));
      insecureCheck->setChecked(settings[kSettingsRpcAllowInsecureKey] == "true");
      fallbackCheck->setChecked(settings[kSettingsProviderFallbackKey] == "true");

      // Logic to show/hide RPC fields
      auto updateVisibility = [=](int index) {
          bool isRpc = providerSelector->itemData(index).toString() == "rpc";
          rpcFrame->setVisible(isRpc);
      };
      connect(providerSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), updateVisibility);
      updateVisibility(providerSelector->currentIndex());

      // Styling inputs
      urlEdit->setStyleSheet(m_themeManager->getLineEditStyleSheet());
      userEdit->setStyleSheet(m_themeManager->getLineEditStyleSheet());
      passEdit->setStyleSheet(m_themeManager->getLineEditStyleSheet());
      
      providerSelector->setStyleSheet(QString(R"(
          QComboBox {
              background-color: %1;
              border: 2px solid %2;
              border-radius: 8px;
              padding: 10px;
              color: %3;
          }
          QComboBox QAbstractItemView {
              background-color: %1;
              color: %3;
              selection-background-color: %4;
          }
      )")
      .arg(m_themeManager->surfaceColor().name())
      .arg(m_themeManager->secondaryColor().name())
      .arg(m_themeManager->textColor().name())
      .arg(m_themeManager->accentColor().name()));

      // Action Buttons
      QHBoxLayout* btnLayout = new QHBoxLayout();
      btnLayout->setSpacing(15);

      QPushButton* testBtn = new QPushButton("Test Connection", card);
      QPushButton* cancelBtn = new QPushButton("Cancel", card);
      QPushButton* saveBtn = new QPushButton("Save Configuration", card);

      saveBtn->setStyleSheet(m_themeManager->getButtonStyleSheet());
      cancelBtn->setStyleSheet(m_themeManager->getButtonStyleSheet());
      testBtn->setStyleSheet(m_themeManager->getButtonStyleSheet());

      btnLayout->addWidget(testBtn);
      btnLayout->addStretch();
      btnLayout->addWidget(cancelBtn);
      btnLayout->addWidget(saveBtn);
      
      cardLayout->addStretch();
      cardLayout->addLayout(btnLayout);
      dialogLayout->addWidget(card);

      // Connect Buttons
      connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
      
      connect(testBtn, &QPushButton::clicked, [=]() {
          // Disable UI during test
          testBtn->setEnabled(false);
          testBtn->setText("Testing...");
          
          QString typeStr = providerSelector->currentData().toString();
          BitcoinProviders::ProviderConfig config;
          
          // Set network to match application default (or make configurable later)
          config.network = "btc/test3";

          if (typeStr == "rpc") {
              config.type = BitcoinProviders::ProviderType::BitcoinRpc;
              config.rpcUrl = urlEdit->text().trimmed().toStdString();
              config.rpcUsername = userEdit->text().trimmed().toStdString();
              config.rpcPassword = passEdit->text().toStdString();
              config.allowInsecureHttp = insecureCheck->isChecked();
              config.enableFallback = fallbackCheck->isChecked();
              
              if (config.rpcUrl.empty()) {
                   QMessageBox::warning(dialog, "Missing URL", "Please enter RPC URL.");
                   testBtn->setEnabled(true);
                   testBtn->setText("Test Connection");
                   return;
              }
          } else {
              config.type = BitcoinProviders::ProviderType::BlockCypher;
              // BlockCypher token could be retrieved from settings if we had a field for it
              // For now, it uses the default (empty/free tier)
          }
          
          // Run test in background to avoid freezing UI
          QFutureWatcher<std::pair<bool, std::string>>* watcher = new QFutureWatcher<std::pair<bool, std::string>>(dialog);
          
          QFuture<std::pair<bool, std::string>> future = QtConcurrent::run([config]() {
              auto provider = BitcoinProviders::CreateProvider(config);
              if (!provider) {
                  return std::make_pair(false, std::string("Failed to create provider configuration. Check settings (e.g. insecure HTTP)."));
              }
              return provider->testConnection();
          });
          
          connect(watcher, &QFutureWatcher<std::pair<bool, std::string>>::finished, [=]() {
              auto result = watcher->result();
              testBtn->setEnabled(true);
              testBtn->setText("Test Connection");
              
              if (result.first) {
                  QMessageBox::information(dialog, "Success", QString::fromStdString(result.second));
              } else {
                  QMessageBox::warning(dialog, "Connection Failed", QString::fromStdString(result.second));
              }
              watcher->deleteLater();
          });
          
          watcher->setFuture(future);
      });

      connect(saveBtn, &QPushButton::clicked, [=]() {
          QString type = providerSelector->currentData().toString();
          m_settingsRepository->setUserSetting(m_currentUserId, kSettingsProviderTypeKey, type.toStdString());
          
          if (type == "rpc") {
              m_settingsRepository->setUserSetting(m_currentUserId, kSettingsRpcUrlKey, urlEdit->text().toStdString());
              m_settingsRepository->setUserSetting(m_currentUserId, kSettingsRpcUsernameKey, userEdit->text().toStdString());
              m_settingsRepository->setUserSetting(m_currentUserId, kSettingsRpcPasswordKey, passEdit->text().toStdString());
              m_settingsRepository->setUserSetting(m_currentUserId, kSettingsRpcAllowInsecureKey, insecureCheck->isChecked() ? "true" : "false");
              m_settingsRepository->setUserSetting(m_currentUserId, kSettingsProviderFallbackKey, fallbackCheck->isChecked() ? "true" : "false");
          }
          
          dialog->accept();
          loadAdvancedSettings(); // Refresh main UI summary
          QMessageBox::information(this, "Saved", "Bitcoin provider settings saved.");
      });

      dialog->exec();
      delete dialog;
  });

  // --- Hardware Wallet Section (Refactored to Floating Dialog) ---
  QFrame* hardwareSection = new QFrame(walletGroup);
  QVBoxLayout* hardwareSectionLayout = new QVBoxLayout(hardwareSection);
  hardwareSectionLayout->setContentsMargins(0, 10, 0, 10);
  hardwareSectionLayout->setSpacing(8);

  QLabel* hardwareHeader = new QLabel("Hardware Wallet (Bitcoin)", hardwareSection);
  hardwareHeader->setStyleSheet(QString("font-weight: 600; font-size: 16px; color: %1;")
      .arg(m_themeManager->textColor().name()));

  m_hardwareStatusLabel = new QLabel("No hardware wallet configured.", hardwareSection);
  m_hardwareStatusLabel->setProperty("class", "subtitle");
  m_hardwareStatusLabel->setWordWrap(true);

  QPushButton* configureHardwareButton = new QPushButton("Configure Hardware Wallet", hardwareSection);
  configureHardwareButton->setObjectName("configureHardwareButton");
  configureHardwareButton->setCursor(Qt::PointingHandCursor);
  configureHardwareButton->setMinimumWidth(200);
  configureHardwareButton->setStyleSheet(m_themeManager->getOutlinedButtonStyleSheet());

  hardwareSectionLayout->addWidget(hardwareHeader);
  hardwareSectionLayout->addWidget(m_hardwareStatusLabel);
  hardwareSectionLayout->addWidget(configureHardwareButton, 0, Qt::AlignLeft);

  walletLayout->addWidget(hardwareSection);

  m_mainLayout->addWidget(walletGroup);

  // Initialize pointers that are no longer in main UI
  m_btcProviderSelector = nullptr;
  m_btcRpcUrlEdit = nullptr;
  m_btcRpcUsernameEdit = nullptr;
  m_btcRpcPasswordEdit = nullptr;
  m_btcAllowInsecureCheck = nullptr;
  m_btcEnableFallbackCheck = nullptr;
  m_btcTestConnectionButton = nullptr;
  m_btcSaveSettingsButton = nullptr;
  
  // These will be managed by the dialog now, but we keep the members for the existing slots if needed
  // or we refactor the slots. For now, init to nullptr.
  m_hardwareWalletSelector = nullptr;
  m_hardwareDerivationPathEdit = nullptr;
  m_hardwareUseTestnetCheck = nullptr;
  m_hardwareDetectButton = nullptr;
  m_hardwareImportXpubButton = nullptr;
  m_hardwareXpubDisplay = nullptr;

  // Connect configuration button
  connect(configureHardwareButton, &QPushButton::clicked, this, [this]() {
      if (!m_settingsRepository || m_currentUserId <= 0) {
          QMessageBox::warning(this, "Not Signed In", "Please sign in to configure hardware wallet.");
          return;
      }

      QDialog* dialog = new QDialog(this);
      dialog->setWindowTitle("Configure Hardware Wallet");
      dialog->setModal(true);

      if (auto* screen = QGuiApplication::primaryScreen()) {
          dialog->resize(screen->size());
      } else {
          dialog->resize(this->size());
      }

      dialog->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
      dialog->setAttribute(Qt::WA_TranslucentBackground);
      dialog->setStyleSheet("QDialog { background-color: rgba(0, 0, 0, 150); }");

      QVBoxLayout* dialogLayout = new QVBoxLayout(dialog);
      dialogLayout->setContentsMargins(0, 0, 0, 0);
      dialogLayout->setAlignment(Qt::AlignCenter);

      QFrame* card = new QFrame(dialog);
      card->setObjectName("hwCard");
      card->setMinimumWidth(500);
      card->setMaximumWidth(600);

      QString cardStyle = QString(R"(
          QFrame#hwCard {
              background-color: %1;
              border-radius: 16px;
              border: 1px solid %2;
          }
      )")
      .arg(m_themeManager->surfaceColor().name())
      .arg(m_themeManager->secondaryColor().name());
      card->setStyleSheet(cardStyle);

      QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(card);
      shadow->setBlurRadius(30);
      shadow->setColor(QColor(0, 0, 0, 80));
      shadow->setOffset(0, 8);
      card->setGraphicsEffect(shadow);

      QVBoxLayout* cardLayout = new QVBoxLayout(card);
      cardLayout->setContentsMargins(30, 30, 30, 30);
      cardLayout->setSpacing(20);

      // Header
      QLabel* titleLabel = new QLabel("Hardware Wallet Setup", card);
      titleLabel->setStyleSheet(QString("font-size: 22px; font-weight: 700; color: %1;").arg(m_themeManager->textColor().name()));
      cardLayout->addWidget(titleLabel);

      QLabel* descLabel = new QLabel("Connect your Trezor, Ledger, or Coldcard via USB.", card);
      descLabel->setWordWrap(true);
      descLabel->setStyleSheet(QString("font-size: 14px; color: %1;").arg(m_themeManager->subtitleColor().name()));
      cardLayout->addWidget(descLabel);

      // Controls
      QComboBox* deviceSelector = new QComboBox(card);
      
      QPushButton* detectBtn = new QPushButton("Detect Devices", card);
      
      QLineEdit* pathEdit = new QLineEdit(card);
      pathEdit->setText("m/44'/0'/0'");
      pathEdit->setPlaceholderText("Derivation Path (e.g. m/84'/0'/0')");
      
      QCheckBox* testnetCheck = new QCheckBox("Use Testnet (btc/test3)", card);
      testnetCheck->setChecked(true);

      QLabel* statusLbl = new QLabel("Connect device and click Detect.", card);
      statusLbl->setStyleSheet(QString("color: %1;").arg(m_themeManager->subtitleColor().name()));

      // Styling
      pathEdit->setStyleSheet(m_themeManager->getLineEditStyleSheet());
      
      deviceSelector->setStyleSheet(QString(R"(
          QComboBox {
              background-color: %1;
              border: 2px solid %2;
              border-radius: 8px;
              padding: 10px;
              color: %3;
          }
          QComboBox QAbstractItemView {
              background-color: %1;
              color: %3;
              selection-background-color: %4;
          }
      )")
      .arg(m_themeManager->surfaceColor().name())
      .arg(m_themeManager->secondaryColor().name())
      .arg(m_themeManager->textColor().name())
      .arg(m_themeManager->accentColor().name()));

      detectBtn->setStyleSheet(m_themeManager->getButtonStyleSheet());

      cardLayout->addWidget(new QLabel("Device Selection:", card));
      QHBoxLayout* selLayout = new QHBoxLayout();
      selLayout->addWidget(deviceSelector, 1);
      selLayout->addWidget(detectBtn);
      cardLayout->addLayout(selLayout);

      cardLayout->addWidget(new QLabel("Derivation Path:", card));
      cardLayout->addWidget(pathEdit);
      cardLayout->addWidget(testnetCheck);
      cardLayout->addWidget(statusLbl);

      // Buttons
      QHBoxLayout* actionLayout = new QHBoxLayout();
      QPushButton* importBtn = new QPushButton("Import Xpub", card);
      QPushButton* cancelBtn = new QPushButton("Close", card);

      importBtn->setStyleSheet(m_themeManager->getButtonStyleSheet());
      cancelBtn->setStyleSheet(m_themeManager->getButtonStyleSheet());

      actionLayout->addStretch();
      actionLayout->addWidget(cancelBtn);
      actionLayout->addWidget(importBtn);
      
      cardLayout->addStretch();
      cardLayout->addLayout(actionLayout);
      dialogLayout->addWidget(card);

      // Logic: Detect Devices
      connect(detectBtn, &QPushButton::clicked, [=]() {
          statusLbl->setText("Scanning for devices...");
          QProcess process;
          process.start("hwi", QStringList() << "enumerate");
          if (!process.waitForFinished(10000)) {
              statusLbl->setText("Error: Detection timed out.");
              return;
          }
          
          QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
          if (output.isEmpty()) {
              statusLbl->setText("No devices found.");
              return;
          }

          QJsonParseError err;
          QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8(), &err);
          if (err.error != QJsonParseError::NoError || !doc.isArray()) {
              statusLbl->setText("Error parsing HWI response.");
              return;
          }

          deviceSelector->clear();
          for (const auto &val : doc.array()) {
              QJsonObject obj = val.toObject();
              QString type = obj["type"].toString();
              QString model = obj["model"].toString();
              QString label = model.isEmpty() ? type : model;
              
              QVariantMap data;
              data["fingerprint"] = obj["fingerprint"].toString();
              deviceSelector->addItem(label, data);
          }
          
          if (deviceSelector->count() > 0) {
              statusLbl->setText(QString("Found %1 device(s).").arg(deviceSelector->count()));
          } else {
              statusLbl->setText("No compatible devices found.");
          }
      });

      // Logic: Import Xpub
      connect(importBtn, &QPushButton::clicked, [=]() {
          if (deviceSelector->count() == 0) {
              statusLbl->setText("Please detect a device first.");
              return;
          }
          
          statusLbl->setText("Requesting public key from device...");
          importBtn->setEnabled(false);
          QApplication::processEvents(); // Update UI

          QVariantMap deviceData = deviceSelector->currentData().toMap();
          QString fingerprint = deviceData["fingerprint"].toString();
          QString path = pathEdit->text().trimmed();
          
          QStringList args;
          if (testnetCheck->isChecked()) args << "--testnet";
          if (!fingerprint.isEmpty()) args << "-f" << fingerprint;
          args << "getxpub" << path;

          QProcess process;
          process.start("hwi", args);
          if (!process.waitForFinished(15000)) {
              statusLbl->setText("Error: Device request timed out.");
              importBtn->setEnabled(true);
              return;
          }

          QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
          QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
          QString xpub = output; // Fallback to raw output
          
          if (doc.isObject() && doc.object().contains("xpub")) {
              xpub = doc.object()["xpub"].toString();
          }

          if (xpub.isEmpty() || xpub.contains("error", Qt::CaseInsensitive)) {
              statusLbl->setText("Failed to retrieve xpub.");
              importBtn->setEnabled(true);
              return;
          }

          // Save to wallet
          auto wallets = m_walletRepository->getWalletsByType(m_currentUserId, "bitcoin", true);
          if (wallets.success && !wallets.data.empty()) {
              auto updateRes = m_walletRepository->updateWallet(wallets.data.front().id, std::nullopt, path.toStdString(), xpub.toStdString());
              if (updateRes.success) {
                  m_hardwareStatusLabel->setText("Hardware wallet configured.");
                  dialog->accept();
                  QMessageBox::information(this, "Success", "Hardware wallet imported successfully.");
              } else {
                  statusLbl->setText("Database error saving wallet.");
              }
          } else {
              statusLbl->setText("No Bitcoin wallet found for user.");
          }
          importBtn->setEnabled(true);
      });

      connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
      dialog->exec();
      delete dialog;
  });

  // Add stretch at the end
  m_mainLayout->addStretch();
}

void QtSettingsUI::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  updateScrollAreaWidth();
}

void QtSettingsUI::updateScrollAreaWidth() {
  if (m_scrollArea && m_leftSpacer && m_rightSpacer) {
    int windowWidth = this->width();
    int windowHeight = this->height();

    if (windowWidth <= 0 || windowHeight <= 0) {
      return;
    }

    // Apply a 55% maximum width for large and ultra-wide screens
    // to keep content centered and readable in the user's focus area.
    if (windowWidth > 1200) {
      int targetWidth = static_cast<int>(windowWidth * 0.55);
      m_scrollArea->setMaximumWidth(targetWidth);
      m_scrollArea->setMinimumWidth(targetWidth);

      // Enable spacers for centering
      m_leftSpacer->changeSize(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
      m_rightSpacer->changeSize(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
    } else {
      // Full width on smaller screens
      m_scrollArea->setMaximumWidth(QWIDGETSIZE_MAX);
      m_scrollArea->setMinimumWidth(0);

      // Disable spacers
      m_leftSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
      m_rightSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    }

    // Ensure layout is refreshed
    if (m_centeringLayout) {
      m_centeringLayout->invalidate();
    }
  }
}

void QtSettingsUI::applyTheme() {
  updateStyles();

  ThemeType currentTheme = m_themeManager->getCurrentTheme();
  int newIndex = m_themeSelector->findData(static_cast<int>(currentTheme));
  if (m_themeSelector->currentIndex() != newIndex) {
    m_themeSelector->blockSignals(true);
    m_themeSelector->setCurrentIndex(newIndex);
    m_themeSelector->blockSignals(false);
  }

  update2FAStatus();
}

void QtSettingsUI::updateStyles() {
  // Early return if theme manager hasn't been set
  if (!m_themeManager) {
    return;
  }

  const QString bgColor = m_themeManager->backgroundColor().name();
  const QString textColor = m_themeManager->textColor().name();
  const QString surfaceColor = m_themeManager->surfaceColor().name();
  const QString accentColor = m_themeManager->accentColor().name();
  const QString subtitleColor = m_themeManager->subtitleColor().name();
  const QString secondaryColor = m_themeManager->secondaryColor().name();

  // Update layout spacing and margins based on new theme
  if (m_mainLayout) {
    m_mainLayout->setContentsMargins(m_themeManager->spacing(6), // 24px
                                     m_themeManager->spacing(5), // 20px
                                     m_themeManager->spacing(6), // 24px
                                     m_themeManager->spacing(5)  // 20px
    );
    m_mainLayout->setSpacing(m_themeManager->spacing(4)); // 16px
  }

  // Style scroll area with explicit background color
  if (m_scrollArea) {
    m_scrollArea->setStyleSheet(QString(R"(
            QScrollArea {
                background-color: %1;
                border: none;
            }
            QScrollBar:vertical {
                background: %1;
                width: 10px;
                border-radius: 5px;
                margin: 2px;
            }
            QScrollBar::handle:vertical {
                background: %2;
                border-radius: 5px;
                min-height: 20px;
            }
            QScrollBar::handle:vertical:hover {
                background: %3;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
                height: 0px;
            }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
                background: none;
            }
        )")
                                    .arg(bgColor)
                                    .arg(secondaryColor)
                                    .arg(accentColor));
  }

  // Style center container with explicit background color
  if (m_centerContainer) {
    m_centerContainer->setStyleSheet(
        QString("QWidget#centerContainer { background-color: %1; }").arg(bgColor));
  }

  // Set main widget background
  this->setStyleSheet(QString("QtSettingsUI { background-color: %1; }").arg(bgColor));

  // Title label styling
  if (m_titleLabel) {
    m_titleLabel->setStyleSheet(
        QString("QLabel { color: %1; background-color: transparent; }")
            .arg(textColor));
    m_titleLabel->setFont(m_themeManager->titleFont());
  }

  // Style 2FA labels directly
  QString labelSubtitleStyle =
      QString("QLabel { color: %1; background-color: transparent; }")
          .arg(subtitleColor);
  QString labelTitleStyle =
      QString("QLabel { color: %1; background-color: transparent; }")
          .arg(textColor);

  if (m_2FATitleLabel) {
    m_2FATitleLabel->setStyleSheet(labelTitleStyle);
    m_2FATitleLabel->setFont(m_themeManager->titleFont());
  }
  if (m_2FADescriptionLabel) {
    m_2FADescriptionLabel->setStyleSheet(labelSubtitleStyle);
  }
  if (m_2FAStatusLabel) {
    m_2FAStatusLabel->setStyleSheet(labelSubtitleStyle);
  }
  if (m_walletPlaceholder) {
    m_walletPlaceholder->setStyleSheet(labelSubtitleStyle);
  }
  if (m_btcProviderStatusLabel) {
    m_btcProviderStatusLabel->setStyleSheet(labelSubtitleStyle);
  }
  if (m_hardwareStatusLabel) {
    m_hardwareStatusLabel->setStyleSheet(labelSubtitleStyle);
  }

  // Group box styling
  QString groupBoxStyle = QString(R"(
        QGroupBox {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 22px;
            font-size: 15px;
            font-weight: 600;
            color: %3;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 3px;
            margin-top: 4px;
        }
    )")
                              .arg(surfaceColor)
                              .arg(secondaryColor)
                              .arg(textColor);

  // Apply to all group boxes
  for (QGroupBox *groupBox : findChildren<QGroupBox *>()) {
    groupBox->setStyleSheet(groupBoxStyle);
  }

  // Combo box styling
  QString comboBoxStyle = QString(R"(
        QComboBox {
            background-color: %1;
            color: %2;
            border: 2px solid %3;
            border-radius: 6px;
            padding: 8px 12px;
            min-width: 200px;
        }
        QComboBox:focus {
            border-color: %4;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 5px solid transparent;
            border-right: 5px solid transparent;
            border-top: 5px solid %2;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            selection-background-color: %4;
        }
    )")
                              .arg(surfaceColor)
                              .arg(textColor)
                              .arg(secondaryColor)
                              .arg(accentColor);

  if (m_themeSelector) {
    m_themeSelector->setStyleSheet(comboBoxStyle);
    m_themeSelector->setFont(m_themeManager->textFont());
  }
  if (m_btcProviderSelector) {
    m_btcProviderSelector->setStyleSheet(comboBoxStyle);
    m_btcProviderSelector->setFont(m_themeManager->textFont());
  }
  if (m_hardwareWalletSelector) {
    m_hardwareWalletSelector->setStyleSheet(comboBoxStyle);
    m_hardwareWalletSelector->setFont(m_themeManager->textFont());
  }

  QString inputStyle = QString(R"(
        QLineEdit {
            background-color: %1;
            color: %2;
            border: 2px solid %3;
            border-radius: 6px;
            padding: 6px 10px;
        }
        QLineEdit:focus {
            border-color: %4;
        }
    )")
                             .arg(surfaceColor)
                             .arg(textColor)
                             .arg(secondaryColor)
                             .arg(accentColor);

  if (m_btcRpcUrlEdit)
    m_btcRpcUrlEdit->setStyleSheet(inputStyle);
  if (m_btcRpcUsernameEdit)
    m_btcRpcUsernameEdit->setStyleSheet(inputStyle);
  if (m_btcRpcPasswordEdit)
    m_btcRpcPasswordEdit->setStyleSheet(inputStyle);
  if (m_hardwareDerivationPathEdit)
    m_hardwareDerivationPathEdit->setStyleSheet(inputStyle);
  if (m_hardwareXpubDisplay)
    m_hardwareXpubDisplay->setStyleSheet(inputStyle);

  QString checkboxStyle =
      QString("QCheckBox { color: %1; } QCheckBox::indicator { border: 1px solid %2; width: 14px; height: 14px; } QCheckBox::indicator:checked { background-color: %3; }")
          .arg(textColor)
          .arg(secondaryColor)
          .arg(accentColor);
  if (m_btcAllowInsecureCheck)
    m_btcAllowInsecureCheck->setStyleSheet(checkboxStyle);
  if (m_btcEnableFallbackCheck)
    m_btcEnableFallbackCheck->setStyleSheet(checkboxStyle);
  if (m_hardwareUseTestnetCheck)
    m_hardwareUseTestnetCheck->setStyleSheet(checkboxStyle);

  // Button styling for 2FA buttons
  QString buttonStyle =
      QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: 2px solid %3;
            border-radius: 8px;
            padding: 8px 16px;
            font-weight: 600;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: %4;
            border-color: %3;
        }
        QPushButton:pressed {
            background-color: %5;
        }
    )")
          .arg(surfaceColor)
          .arg(textColor)
          .arg(accentColor)
          .arg(m_themeManager->secondaryColor().lighter(110).name())
          .arg(m_themeManager->accentColor().darker(120).name());

  if (m_enable2FAButton) {
    m_enable2FAButton->setStyleSheet(buttonStyle);
  }
  if (m_disable2FAButton) {
    m_disable2FAButton->setStyleSheet(buttonStyle);
  }
  
  // Update outlined buttons
  QPushButton* providerBtn = findChild<QPushButton*>("configureProviderButton");
  if (providerBtn) {
      providerBtn->setStyleSheet(m_themeManager->getOutlinedButtonStyleSheet());
  }
  
  QPushButton* hardwareBtn = findChild<QPushButton*>("configureHardwareButton");
  if (hardwareBtn) {
      hardwareBtn->setStyleSheet(m_themeManager->getOutlinedButtonStyleSheet());
  }

  if (m_btcTestConnectionButton) {
    m_btcTestConnectionButton->setStyleSheet(buttonStyle);
  }
  if (m_btcSaveSettingsButton) {
    m_btcSaveSettingsButton->setStyleSheet(buttonStyle);
  }
  if (m_hardwareDetectButton) {
    m_hardwareDetectButton->setStyleSheet(buttonStyle);
  }
  if (m_hardwareImportXpubButton) {
    m_hardwareImportXpubButton->setStyleSheet(buttonStyle);
  }

  // Force visual refresh
  update();
}

void QtSettingsUI::update2FAStatus() {
  if (g_currentUser.empty()) {
    if (m_2FAStatusLabel) {
      m_2FAStatusLabel->setText("Please sign in to manage 2FA settings.");
    }
    if (m_enable2FAButton)
      m_enable2FAButton->hide();
    if (m_disable2FAButton)
      m_disable2FAButton->hide();
    return;
  }

  bool isEnabled = Auth::IsTwoFactorEnabled(g_currentUser);

  if (isEnabled) {
    if (m_2FAStatusLabel) {
      m_2FAStatusLabel->setText("✓ Two-factor authentication is enabled");
      // We can use dynamic properties for semantic coloring if needed
      m_2FAStatusLabel->setProperty("status", "success");
      m_2FAStatusLabel->style()->unpolish(m_2FAStatusLabel);
      m_2FAStatusLabel->style()->polish(m_2FAStatusLabel);
    }
    if (m_enable2FAButton)
      m_enable2FAButton->hide();
    if (m_disable2FAButton) {
      m_disable2FAButton->show();
      m_disable2FAButton->setEnabled(true);
    }
  } else {
    if (m_2FAStatusLabel) {
      m_2FAStatusLabel->setText("Two-factor authentication is disabled");
      m_2FAStatusLabel->setProperty("status", "normal");
      m_2FAStatusLabel->style()->unpolish(m_2FAStatusLabel);
      m_2FAStatusLabel->style()->polish(m_2FAStatusLabel);
    }
    if (m_enable2FAButton) {
      m_enable2FAButton->show();
      m_enable2FAButton->setEnabled(true);
    }
    if (m_disable2FAButton)
      m_disable2FAButton->hide();
  }
}

void QtSettingsUI::refresh2FAStatus() {
  update2FAStatus();
  loadAdvancedSettings();
}

void QtSettingsUI::loadAdvancedSettings() {
  if (!m_btcProviderSelector || !m_btcRpcUrlEdit || !m_btcRpcUsernameEdit ||
      !m_btcRpcPasswordEdit || !m_btcAllowInsecureCheck ||
      !m_btcEnableFallbackCheck || !m_hardwareXpubDisplay ||
      !m_btcProviderStatusLabel || !m_hardwareStatusLabel) {
    return;
  }

  if (!m_settingsRepository || m_currentUserId <= 0) {
    m_btcProviderSelector->setCurrentIndex(0);
    m_btcRpcUrlEdit->clear();
    m_btcRpcUsernameEdit->clear();
    m_btcRpcPasswordEdit->clear();
    m_btcAllowInsecureCheck->setChecked(true);
    m_btcEnableFallbackCheck->setChecked(true);
    m_btcProviderStatusLabel->setText(
        "Sign in to save provider settings.");
    m_hardwareXpubDisplay->clear();
    m_hardwareStatusLabel->setText("Sign in to manage hardware wallets.");
    return;
  }

  std::vector<std::string> keys = {kSettingsProviderTypeKey, kSettingsRpcUrlKey,
                                   kSettingsRpcUsernameKey, kSettingsRpcPasswordKey,
                                   kSettingsRpcAllowInsecureKey, kSettingsProviderFallbackKey};
  auto settingsResult =
      m_settingsRepository->getUserSettings(m_currentUserId, keys);
  std::map<std::string, std::string> settings;
  if (settingsResult.success) {
    settings = settingsResult.data;
  }

  QString provider = "blockcypher";
  auto providerIt = settings.find(kSettingsProviderTypeKey);
  if (providerIt != settings.end()) {
    provider = QString::fromStdString(providerIt->second);
  }

  int providerIndex = m_btcProviderSelector->findData(provider);
  if (providerIndex >= 0) {
    m_btcProviderSelector->setCurrentIndex(providerIndex);
  }

  auto urlIt = settings.find(kSettingsRpcUrlKey);
  m_btcRpcUrlEdit->setText(
      urlIt != settings.end() ? QString::fromStdString(urlIt->second)
                              : QString());

  auto userIt = settings.find(kSettingsRpcUsernameKey);
  m_btcRpcUsernameEdit->setText(
      userIt != settings.end() ? QString::fromStdString(userIt->second)
                               : QString());

  auto passIt = settings.find(kSettingsRpcPasswordKey);
  m_btcRpcPasswordEdit->setText(
      passIt != settings.end() ? QString::fromStdString(passIt->second)
                               : QString());

  auto allowIt = settings.find(kSettingsRpcAllowInsecureKey);
  bool allowInsecure = allowIt == settings.end() ||
                       allowIt->second == "true" || allowIt->second == "1";
  m_btcAllowInsecureCheck->setChecked(allowInsecure);

  auto fallbackIt = settings.find(kSettingsProviderFallbackKey);
  bool allowFallback = fallbackIt == settings.end() ||
                       fallbackIt->second == "true" || fallbackIt->second == "1";
  m_btcEnableFallbackCheck->setChecked(allowFallback);

  m_btcProviderStatusLabel->setText(
      "Provider settings loaded for this device.");

  if (m_walletRepository) {
    auto walletResult =
        m_walletRepository->getWalletsByType(m_currentUserId, "bitcoin", true);
    if (walletResult.success && !walletResult.data.empty()) {
      const auto &wallet = walletResult.data.front();
      if (wallet.extendedPublicKey.has_value()) {
        m_hardwareXpubDisplay->setText(
            QString::fromStdString(*wallet.extendedPublicKey));
        m_hardwareStatusLabel->setText("Hardware wallet xpub imported.");
      } else {
        m_hardwareXpubDisplay->clear();
        m_hardwareStatusLabel->setText("No hardware wallet xpub stored.");
      }
    }
  }
}

void QtSettingsUI::updateHardwareWalletStatus(const QString &message,
                                              bool success) {
  if (m_hardwareStatusLabel) {
    QString prefix = success ? "✓ " : "";
    m_hardwareStatusLabel->setText(prefix + message);
  }
}

void QtSettingsUI::onSaveAdvancedSettings() {
  if (!m_settingsRepository || m_currentUserId <= 0) {
    QMessageBox::warning(this, "Not Signed In",
                         "Please sign in to save settings.");
    return;
  }

  QString providerType =
      m_btcProviderSelector->currentData().toString().trimmed();
  QString rpcUrl = m_btcRpcUrlEdit->text().trimmed();
  QString rpcUsername = m_btcRpcUsernameEdit->text().trimmed();
  QString rpcPassword = m_btcRpcPasswordEdit->text();
  bool allowInsecure = m_btcAllowInsecureCheck->isChecked();
  bool allowFallback = m_btcEnableFallbackCheck->isChecked();

  if (providerType == "rpc") {
    if (rpcUrl.isEmpty()) {
      QMessageBox::warning(this, "Missing RPC URL",
                           "Please enter the RPC URL for your node.");
      return;
    }

    if (rpcUrl.startsWith("http://") && !allowInsecure) {
      QMessageBox::warning(
          this, "Insecure RPC URL",
          "This RPC URL uses HTTP. Enable 'Allow HTTP for local node' or use "
          "HTTPS.");
      return;
    }
  }

  auto saveResult =
      m_settingsRepository->setUserSetting(m_currentUserId, kSettingsProviderTypeKey,
                                           providerType.toStdString());
  if (!saveResult.success) {
    QMessageBox::warning(this, "Save Failed", "Failed to save provider type.");
    return;
  }

  m_settingsRepository->setUserSetting(m_currentUserId, kSettingsRpcUrlKey,
                                       rpcUrl.toStdString());
  m_settingsRepository->setUserSetting(m_currentUserId, kSettingsRpcUsernameKey,
                                       rpcUsername.toStdString());
  m_settingsRepository->setUserSetting(m_currentUserId, kSettingsRpcPasswordKey,
                                       rpcPassword.toStdString());
  m_settingsRepository->setUserSetting(m_currentUserId, kSettingsRpcAllowInsecureKey,
                                       allowInsecure ? "true" : "false");
  m_settingsRepository->setUserSetting(m_currentUserId, kSettingsProviderFallbackKey,
                                       allowFallback ? "true" : "false");

  m_btcProviderStatusLabel->setText(
      "Provider settings saved on this device.");

  emit bitcoinProviderSettingsChanged(providerType, rpcUrl, rpcUsername,
                                      rpcPassword, allowInsecure,
                                      allowFallback);
}

void QtSettingsUI::onTestRpcConnection() {
  QString rpcUrl = m_btcRpcUrlEdit->text().trimmed();
  if (rpcUrl.isEmpty()) {
    QMessageBox::warning(this, "Missing RPC URL",
                         "Please enter the RPC URL to test.");
    return;
  }

  if (rpcUrl.startsWith("http://") &&
      !m_btcAllowInsecureCheck->isChecked()) {
    QMessageBox::warning(this, "Insecure RPC URL",
                         "Enable HTTP for local node or use HTTPS.");
    return;
  }

  QNetworkRequest rpcNetworkRequest{QUrl(rpcUrl)};
  rpcNetworkRequest.setRawHeader("Content-Type", "application/json");

  QString username = m_btcRpcUsernameEdit->text().trimmed();
  QString password = m_btcRpcPasswordEdit->text();
  if (!username.isEmpty()) {
    QByteArray credentials =
        QString("%1:%2").arg(username, password).toUtf8().toBase64();
    rpcNetworkRequest.setRawHeader("Authorization", "Basic " + credentials);
  }

  QJsonObject payload;
  payload.insert("jsonrpc", "1.0");
  payload.insert("id", "criptogualet");
  payload.insert("method", "getblockchaininfo");
  payload.insert("params", QJsonArray());

  QNetworkAccessManager manager;
  QEventLoop loop;
  QTimer timeoutTimer;
  timeoutTimer.setSingleShot(true);
  timeoutTimer.setInterval(10000);

  QNetworkReply *reply =
      manager.post(rpcNetworkRequest, QJsonDocument(payload).toJson());
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
  timeoutTimer.start();
  loop.exec();

  if (!timeoutTimer.isActive()) {
    reply->abort();
    reply->deleteLater();
    QMessageBox::warning(this, "RPC Timeout",
                         "RPC request timed out after 10 seconds.");
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
    QString errorMessage = reply->errorString();
    reply->deleteLater();
    QMessageBox::warning(this, "RPC Error",
                         "RPC request failed: " + errorMessage);
    return;
  }

  QByteArray responseData = reply->readAll();
  reply->deleteLater();

  QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
  if (!responseDoc.isObject() ||
      !responseDoc.object().contains("result")) {
    QMessageBox::warning(this, "RPC Error",
                         "RPC response missing expected data.");
    return;
  }

  QMessageBox::information(this, "RPC Connected",
                           "Successfully connected to your RPC node.");
  if (m_btcProviderStatusLabel) {
    m_btcProviderStatusLabel->setText("RPC connection successful.");
  }
}

void QtSettingsUI::onDetectHardwareWallets() {
  QProcess process;
  process.start("hwi", QStringList() << "enumerate");
  if (!process.waitForFinished(10000)) {
    updateHardwareWalletStatus("Hardware wallet detection timed out.", false);
    return;
  }

  QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QString errorOutput =
      QString::fromUtf8(process.readAllStandardError()).trimmed();

  if (output.isEmpty()) {
    updateHardwareWalletStatus(
        errorOutput.isEmpty() ? "No hardware wallets detected."
                              : errorOutput,
        false);
    return;
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
    updateHardwareWalletStatus("Unable to parse hardware wallet list.", false);
    return;
  }

  m_hardwareWalletSelector->clear();
  for (const auto &deviceValue : doc.array()) {
    if (!deviceValue.isObject()) {
      continue;
    }
    QJsonObject deviceObj = deviceValue.toObject();
    QString type = deviceObj.value("type").toString();
    QString model = deviceObj.value("model").toString();
    QString fingerprint = deviceObj.value("fingerprint").toString();
    QString path = deviceObj.value("path").toString();

    QString label = model.isEmpty() ? type : model + " (" + type + ")";
    if (label.trimmed().isEmpty()) {
      label = "Hardware Wallet";
    }

    QVariantMap data;
    data.insert("type", type);
    data.insert("model", model);
    data.insert("fingerprint", fingerprint);
    data.insert("path", path);

    m_hardwareWalletSelector->addItem(label, data);
  }

  if (m_hardwareWalletSelector->count() == 0) {
    updateHardwareWalletStatus("No compatible hardware wallets found.", false);
    return;
  }

  updateHardwareWalletStatus("Hardware wallet detected. Select to import xpub.",
                             true);
}

void QtSettingsUI::onImportHardwareXpub() {
  if (!m_walletRepository || m_currentUserId <= 0) {
    QMessageBox::warning(this, "Not Signed In",
                         "Please sign in to import a hardware wallet xpub.");
    return;
  }

  QVariant deviceData = m_hardwareWalletSelector->currentData();
  if (!deviceData.isValid()) {
    QMessageBox::warning(this, "No Device",
                         "Please detect and select a hardware wallet.");
    return;
  }

  QVariantMap device = deviceData.toMap();
  QString fingerprint = device.value("fingerprint").toString();
  QString derivationPath = m_hardwareDerivationPathEdit->text().trimmed();
  if (derivationPath.isEmpty()) {
    QMessageBox::warning(this, "Missing Path",
                         "Please enter a derivation path.");
    return;
  }

  QStringList args;
  if (m_hardwareUseTestnetCheck->isChecked()) {
    args << "--testnet";
  }
  if (!fingerprint.isEmpty()) {
    args << "-f" << fingerprint;
  }
  args << "getxpub" << derivationPath;

  QProcess process;
  process.start("hwi", args);
  if (!process.waitForFinished(15000)) {
    updateHardwareWalletStatus("Hardware wallet request timed out.", false);
    return;
  }

  QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
  QString errorOutput =
      QString::fromUtf8(process.readAllStandardError()).trimmed();

  if (output.isEmpty()) {
    updateHardwareWalletStatus(
        errorOutput.isEmpty() ? "Failed to retrieve xpub."
                              : errorOutput,
        false);
    return;
  }

  QString xpub = output;
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8(), &parseError);
  if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
    QJsonObject obj = doc.object();
    if (obj.contains("xpub")) {
      xpub = obj.value("xpub").toString();
    }
  }

  if (xpub.isEmpty()) {
    updateHardwareWalletStatus("Hardware wallet returned an empty xpub.", false);
    return;
  }

  auto walletResult =
      m_walletRepository->getWalletsByType(m_currentUserId, "bitcoin", true);
  if (!walletResult.success || walletResult.data.empty()) {
    QMessageBox::warning(this, "Wallet Missing",
                         "No Bitcoin wallet found to store the xpub.");
    return;
  }

  const auto &wallet = walletResult.data.front();
  auto updateResult = m_walletRepository->updateWallet(
      wallet.id, std::nullopt, derivationPath.toStdString(), xpub.toStdString());
  if (!updateResult.success) {
    QMessageBox::warning(this, "Update Failed",
                         "Failed to store hardware wallet xpub.");
    return;
  }

  m_hardwareXpubDisplay->setText(xpub);
  updateHardwareWalletStatus("Hardware wallet xpub imported successfully.",
                             true);
}

void QtSettingsUI::onEnable2FAClicked() {
  if (g_currentUser.empty()) {
    QMessageBox::warning(this, "Not Signed In",
                         "Please sign in to manage 2FA settings.");
    return;
  }

  // Step 1: Password confirmation dialog
  QtPasswordConfirmDialog passwordDialog(
      QString::fromStdString(g_currentUser), "Enable Two-Factor Authentication",
      "Please enter your password to enable 2FA:", this);

  if (passwordDialog.exec() == QDialog::Accepted &&
      passwordDialog.isConfirmed()) {
    QString password = passwordDialog.getPassword();

    // Step 2: Generate TOTP secret and get QR code URI
    Auth::TwoFactorSetupData setupData =
        Auth::InitiateTwoFactorSetup(g_currentUser, password.toStdString());

    if (!setupData.success) {
      QMessageBox::warning(
          this, "Error",
          QString("Failed to initialize 2FA: %1")
              .arg(QString::fromStdString(setupData.errorMessage)));
      return;
    }

    // Step 3: Show QR code dialog for scanning
    QDialog setupDialog(this);
    setupDialog.setWindowTitle("Set Up Two-Factor Authentication");
    setupDialog.setModal(true);
    setupDialog.setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout(&setupDialog);
    layout->setSpacing(15);
    layout->setContentsMargins(20, 20, 20, 20);

    // Instructions
    QLabel *instructionsLabel = new QLabel(
        "Scan this QR code with your authenticator app\n"
        "(Google Authenticator, Authy, Microsoft Authenticator, etc.)",
        &setupDialog);
    instructionsLabel->setAlignment(Qt::AlignCenter);
    instructionsLabel->setWordWrap(true);
    layout->addWidget(instructionsLabel);

    // QR Code
    QLabel *qrLabel = new QLabel(&setupDialog);
    qrLabel->setAlignment(Qt::AlignCenter);
    qrLabel->setMinimumSize(200, 200);

    // Generate QR code from the otpauth URI
    QR::QRData qrData;
    if (QR::GenerateQRCode(setupData.otpauthUri, qrData) && qrData.width > 0) {
      // Convert QR data to QImage
      int scale = 200 / qrData.width;
      if (scale < 1)
        scale = 1;
      int imgSize = qrData.width * scale;
      QImage qrImage(imgSize, imgSize, QImage::Format_RGB32);
      qrImage.fill(Qt::white);

      for (int y = 0; y < qrData.height; ++y) {
        for (int x = 0; x < qrData.width; ++x) {
          if (qrData.data[y * qrData.width + x]) {
            // Fill a scaled block
            for (int sy = 0; sy < scale; ++sy) {
              for (int sx = 0; sx < scale; ++sx) {
                qrImage.setPixel(x * scale + sx, y * scale + sy, qRgb(0, 0, 0));
              }
            }
          }
        }
      }
      qrLabel->setPixmap(QPixmap::fromImage(qrImage));
    } else {
      qrLabel->setText("QR code generation failed.\nUse manual entry below.");
    }
    layout->addWidget(qrLabel);

    // Manual entry option
    QLabel *manualLabel =
        new QLabel("Or enter this code manually:", &setupDialog);
    manualLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(manualLabel);

    // Secret key display with copy button
    QHBoxLayout *secretLayout = new QHBoxLayout();
    QLineEdit *secretEdit = new QLineEdit(&setupDialog);
    secretEdit->setText(QString::fromStdString(setupData.secretBase32));
    secretEdit->setReadOnly(true);
    secretEdit->setAlignment(Qt::AlignCenter);
    secretEdit->setFont(QFont("Courier", 11));
    secretLayout->addWidget(secretEdit);

    QPushButton *copyButton = new QPushButton("Copy", &setupDialog);
    copyButton->setMaximumWidth(60);
    connect(copyButton, &QPushButton::clicked, [secretEdit]() {
      QGuiApplication::clipboard()->setText(secretEdit->text());
    });
    secretLayout->addWidget(copyButton);
    layout->addLayout(secretLayout);

    // Verification code entry
    layout->addSpacing(10);
    QLabel *verifyLabel = new QLabel(
        "Enter the 6-digit code from your app to verify:", &setupDialog);
    verifyLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(verifyLabel);

    QLineEdit *codeEdit = new QLineEdit(&setupDialog);
    codeEdit->setMaxLength(6);
    codeEdit->setAlignment(Qt::AlignCenter);
    codeEdit->setPlaceholderText("000000");
    codeEdit->setFont(QFont("Courier", 16));
    layout->addWidget(codeEdit);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &setupDialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText("Verify & Enable");
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &setupDialog,
            &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &setupDialog,
            &QDialog::reject);

    if (setupDialog.exec() == QDialog::Accepted) {
      QString code = codeEdit->text().trimmed();

      if (code.isEmpty() || code.length() != 6) {
        QMessageBox::warning(this, "Invalid Code",
                             "Please enter a valid 6-digit code.");
        return;
      }

      // Verify and enable 2FA
      Auth::AuthResponse confirmResult =
          Auth::ConfirmTwoFactorSetup(g_currentUser, code.toStdString());

      if (confirmResult.success()) {
        // Show backup codes
        Auth::BackupCodesResult backupCodes =
            Auth::GetBackupCodes(g_currentUser, password.toStdString());

        QString backupMessage =
            "Two-factor authentication has been enabled!\n\n";
        if (backupCodes.success && !backupCodes.codes.empty()) {
          backupMessage += "Save these backup codes in a secure location:\n\n";
          for (const auto &code : backupCodes.codes) {
            backupMessage += QString::fromStdString(code) + "\n";
          }
          backupMessage += "\nEach code can only be used once.";
        }

        QMessageBox::information(this, "2FA Enabled", backupMessage);
        update2FAStatus();
      } else {
        QMessageBox::warning(
            this, "Verification Failed",
            QString("Invalid code: %1")
                .arg(QString::fromStdString(confirmResult.message)));
      }
    }
  }
}

void QtSettingsUI::onDisable2FAClicked() {
  if (g_currentUser.empty()) {
    QMessageBox::warning(this, "Not Signed In",
                         "Please sign in to manage 2FA settings.");
    return;
  }

  // Confirm with user
  int ret = QMessageBox::question(
      this, "Disable 2FA",
      "Are you sure you want to disable two-factor authentication?\n\n"
      "This will reduce the security of your account. You can re-enable it "
      "later through the settings.",
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (ret == QMessageBox::Yes) {
    // Create dialog for password and TOTP code
    QDialog disableDialog(this);
    disableDialog.setWindowTitle("Disable Two-Factor Authentication");
    disableDialog.setModal(true);
    disableDialog.setMinimumWidth(350);

    QVBoxLayout *layout = new QVBoxLayout(&disableDialog);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);

    QLabel *instructionLabel = new QLabel(
        "Enter your password and current authenticator code\nto disable 2FA:",
        &disableDialog);
    instructionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(instructionLabel);

    // Password field
    QLabel *passwordLabel = new QLabel("Password:", &disableDialog);
    layout->addWidget(passwordLabel);
    QLineEdit *passwordEdit = new QLineEdit(&disableDialog);
    passwordEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(passwordEdit);

    // TOTP code field
    QLabel *codeLabel = new QLabel("Authenticator Code:", &disableDialog);
    layout->addWidget(codeLabel);
    QLineEdit *codeEdit = new QLineEdit(&disableDialog);
    codeEdit->setMaxLength(6);
    codeEdit->setPlaceholderText("000000");
    layout->addWidget(codeEdit);

    // Backup code option
    QLabel *backupLabel =
        new QLabel("<small>Lost your authenticator? <a href='backup'>Use a "
                   "backup code</a></small>",
                   &disableDialog);
    backupLabel->setTextFormat(Qt::RichText);
    backupLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(backupLabel);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &disableDialog);
    buttonBox->button(QDialogButtonBox::Ok)->setText("Disable 2FA");
    layout->addWidget(buttonBox);

    bool useBackupCode = false;
    connect(backupLabel, &QLabel::linkActivated,
            [&useBackupCode, codeLabel, codeEdit]() {
              useBackupCode = true;
              codeLabel->setText("Backup Code:");
              codeEdit->setMaxLength(8);
              codeEdit->setPlaceholderText("xxxxxxxx");
              codeEdit->clear();
            });

    connect(buttonBox, &QDialogButtonBox::accepted, &disableDialog,
            &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &disableDialog,
            &QDialog::reject);

    if (disableDialog.exec() == QDialog::Accepted) {
      QString password = passwordEdit->text();
      QString code = codeEdit->text().trimmed();

      if (password.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter your password.");
        return;
      }

      if (code.isEmpty()) {
        QMessageBox::warning(this, "Error",
                             "Please enter your authenticator code.");
        return;
      }

      Auth::AuthResponse response;
      if (useBackupCode) {
        response = Auth::UseBackupCode(g_currentUser, code.toStdString());
      } else {
        response = Auth::DisableTwoFactor(g_currentUser, password.toStdString(),
                                          code.toStdString());
      }

      if (response.success()) {
        QMessageBox::information(
            this, "2FA Disabled",
            "Two-factor authentication has been disabled successfully.");
        update2FAStatus();
      } else {
        QMessageBox::warning(
            this, "Error",
            QString("Failed to disable 2FA: %1")
                .arg(QString::fromStdString(response.message)));
        update2FAStatus();
      }
    }
  }
}
