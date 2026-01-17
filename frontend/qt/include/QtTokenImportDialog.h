#pragma once

#include <QDialog>
#include <optional>
#include <QPaintEvent>

#include "EthereumService.h"

// Forward declarations
class QLineEdit;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QFrame;
class QtThemeManager;

#include "WalletAPI.h"

class QtTokenImportDialog : public QDialog {
    Q_OBJECT

public:
    struct ImportData {
        QString contractAddress;
    };

    explicit QtTokenImportDialog(QWidget* parent = nullptr);
    ~QtTokenImportDialog();

    std::optional<ImportData> getImportData() const;
    void setEthereumWallet(WalletAPI::EthereumWallet* ethereumWallet);
    void setThemeManager(QtThemeManager* themeManager);

signals:
    void tokenInfoFetched(bool success, const EthereumService::TokenInfo& tokenInfo);

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onAddressChanged(const QString& address);
    void onTokenInfoFetched(bool success, const EthereumService::TokenInfo& tokenInfo);
    void applyTheme();

private:
    void setupUI();
    void fetchTokenInfo(const QString& address);

    WalletAPI::EthereumWallet* m_ethereumWallet;
    QtThemeManager* m_themeManager;

    QLineEdit* m_addressInput;
    QLabel* m_nameLabel;
    QLabel* m_symbolLabel;
    QLabel* m_decimalsLabel;
    QLabel* m_nameValueLabel;
    QLabel* m_symbolValueLabel;
    QLabel* m_decimalsValueLabel;
    QLabel* m_statusLabel;
    QPushButton* m_importButton;
    QPushButton* m_cancelButton;
    QFrame* m_infoFrame;
    QLabel* m_titleLabel;
    QLabel* m_addressLabel;

    std::optional<ImportData> m_importData;
    QString m_lastQueriedAddress;
};