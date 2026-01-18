#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class QtThemeManager;

struct TokenCardData {
    QString contractAddress;
    QString name;
    QString symbol;
    int decimals;
    QString balance;
    QString balanceUSD;
};

class QtTokenCard : public QFrame {
    Q_OBJECT

public:
    explicit QtTokenCard(QtThemeManager* themeManager, QWidget* parent = nullptr);
    ~QtTokenCard() override = default;

    void setTokenData(const TokenCardData& tokenData);
    void setTokenData(const QString& contractAddress, const QString& name, const QString& symbol, int decimals);
    void setBalance(const QString& balance);
    void setBalanceUSD(const QString& balanceUSD);
    void applyTheme();

    TokenCardData getTokenData() const;

signals:
    void tokenClicked(const QString& contractAddress);
    void deleteTokenClicked(const QString& contractAddress);
    void sendTokenClicked(const QString& contractAddress);

private slots:
    void onCardClicked();
    void onDeleteClicked();
    void onSendClicked();
    void onIconDownloaded(QNetworkReply* reply);
    void onThemeChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void setupUI();
    void updateStyles();
    QString getTokenIconUrl(const QString& symbol);
    void setFallbackIcon();

    QtThemeManager* m_themeManager;
    QNetworkAccessManager* m_networkManager;

    TokenCardData m_tokenData;

    QFrame* m_container;
    QHBoxLayout* m_mainLayout;
    QLabel* m_tokenIcon;
    QLabel* m_tokenSymbol;
    QLabel* m_tokenName;
    QLabel* m_tokenBalance;
    QLabel* m_tokenBalanceUSD;
    QPushButton* m_deleteButton;
    QPushButton* m_sendButton;

    bool m_isHovered;
};
