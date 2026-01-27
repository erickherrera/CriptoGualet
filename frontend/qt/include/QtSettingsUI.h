#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

namespace Repository {
class SettingsRepository;
class UserRepository;
class WalletRepository;
}

class QtThemeManager;

class QtSettingsUI : public QWidget {
    Q_OBJECT

  public:
    explicit QtSettingsUI(QWidget* parent = nullptr);
    ~QtSettingsUI() override = default;

    void applyTheme();
    void refresh2FAStatus();
    void setRepositories(Repository::UserRepository* userRepository,
                         Repository::WalletRepository* walletRepository,
                         Repository::SettingsRepository* settingsRepository);
    void setCurrentUserId(int userId);

  signals:
    void bitcoinProviderSettingsChanged(const QString& providerType,
                                        const QString& rpcUrl,
                                        const QString& rpcUsername,
                                        const QString& rpcPassword,
                                        bool allowInsecureHttp,
                                        bool enableFallback);

  private slots:
    void onEnable2FAClicked();
    void onDisable2FAClicked();
    void onSaveAdvancedSettings();
    void onTestRpcConnection();
    void onDetectHardwareWallets();
    void onImportHardwareXpub();

  private:
    void setupUI();
    void update2FAStatus();
    void updateStyles();
    void loadAdvancedSettings();
    void updateHardwareWalletStatus(const QString& message, bool success);

    QtThemeManager* m_themeManager;
    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_centerContainer;
    QLabel* m_titleLabel;
    QLabel* m_walletPlaceholder;
    QComboBox* m_themeSelector;

    QLabel* m_2FATitleLabel;
    QLabel* m_2FAStatusLabel;
    QPushButton* m_enable2FAButton;
    QPushButton* m_disable2FAButton;
    QLabel* m_2FADescriptionLabel;

    Repository::UserRepository* m_userRepository;
    Repository::WalletRepository* m_walletRepository;
    Repository::SettingsRepository* m_settingsRepository;
    int m_currentUserId;

    QComboBox* m_btcProviderSelector;
    QLineEdit* m_btcRpcUrlEdit;
    QLineEdit* m_btcRpcUsernameEdit;
    QLineEdit* m_btcRpcPasswordEdit;
    QCheckBox* m_btcAllowInsecureCheck;
    QCheckBox* m_btcEnableFallbackCheck;
    QLabel* m_btcProviderStatusLabel;
    QPushButton* m_btcTestConnectionButton;
    QPushButton* m_btcSaveSettingsButton;

    QComboBox* m_hardwareWalletSelector;
    QLineEdit* m_hardwareDerivationPathEdit;
    QCheckBox* m_hardwareUseTestnetCheck;
    QPushButton* m_hardwareDetectButton;
    QPushButton* m_hardwareImportXpubButton;
    QLabel* m_hardwareStatusLabel;
    QLineEdit* m_hardwareXpubDisplay;
};
