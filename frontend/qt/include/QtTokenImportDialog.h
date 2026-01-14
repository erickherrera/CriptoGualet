#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QScrollArea>
#include <QWidget>
#include <optional>

#include "../../../backend/blockchain/include/EthereumService.h"
#include "../../../backend/core/include/WalletAPI.h"

class QtThemeManager;

struct TokenImportData {
    QString contractAddress;
    QString name;
    QString symbol;
    int decimals;
};

class QtTokenImportDialog : public QDialog {
    Q_OBJECT

public:
    explicit QtTokenImportDialog(QWidget* parent = nullptr);
    ~QtTokenImportDialog() override = default;

    std::optional<TokenImportData> getImportData() const;
    void setEthereumWallet(WalletAPI::EthereumWallet* wallet);
    void setThemeManager(QtThemeManager* themeManager);

signals:
    void tokenInfoFetched(const EthereumService::TokenInfo& info);

private slots:
    void onAddressChanged(const QString& text);
    void onFetchTokenInfo();
    void onImportClicked();
    void onCancelClicked();
    void onSuggestedTokenClicked(const QString& contractAddress);

private:
    void setupUI();
    void setupPopularTokens();
    void applyTheme();
    void validateAddress(const QString& address);
    void showPreview(const EthereumService::TokenInfo& info);
    void showError(const QString& message);
    void showLoading(bool loading);
    void clearPreview();

    bool isValidEthereumAddress(const QString& address) const;

    QtThemeManager* m_themeManager;
    WalletAPI::EthereumWallet* m_ethereumWallet;

    std::optional<TokenImportData> m_importData;

    QScrollArea* m_scrollArea;
    QWidget* m_contentWidget;
    QVBoxLayout* m_mainLayout;

    QLabel* m_titleLabel;
    QLabel* m_subtitleLabel;
    QLineEdit* m_addressInput;
    QLabel* m_addressError;
    QPushButton* m_fetchButton;
    QProgressBar* m_loadingBar;

    QLabel* m_previewTitle;
    QWidget* m_previewWidget;
    QLabel* m_tokenNameLabel;
    QLabel* m_tokenSymbolLabel;
    QLabel* m_tokenDecimalsLabel;
    QLabel* m_tokenAddressLabel;

    QLabel* m_popularTokensTitle;
    QWidget* m_popularTokensWidget;
    QVBoxLayout* m_popularTokensLayout;

    QHBoxLayout* m_buttonLayout;
    QPushButton* m_importButton;
    QPushButton* m_cancelButton;

    bool m_isValidAddress;
    bool m_hasTokenInfo;
};
