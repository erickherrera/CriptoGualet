#pragma once

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

#include "QtTokenCard.h"

class QtThemeManager;

class QtTokenListWidget : public QWidget {
    Q_OBJECT

  public:
    explicit QtTokenListWidget(QtThemeManager* themeManager, QWidget* parent = nullptr);
    ~QtTokenListWidget() override = default;

    void addToken(const TokenCardData& tokenData);
    void removeToken(const QString& contractAddress);
    void updateTokenBalance(const QString& contractAddress, const QString& balance,
                            const QString& balanceUSD = QString());
    void clearTokens();
    void applyTheme();
    void setEmptyMessage(const QString& message);

  signals:
    void tokenClicked(const QString& contractAddress);
    void deleteTokenClicked(const QString& contractAddress);
    void sendTokenClicked(const QString& contractAddress);
    void importTokenRequested();

  private slots:
    void onTokenClicked(const QString& contractAddress);
    void onDeleteTokenClicked(const QString& contractAddress);
    void onSendTokenClicked(const QString& contractAddress);
    void onImportClicked();
    void onSearchChanged(const QString& text);
    void onSortChanged(int index);

  private:
    void setupUI();
    void updateTokenVisibility();
    void showEmptyState();
    void showTokenList();
    QString calculateTokenUSD(const QString& balance, const QString& symbol);

    QtThemeManager* m_themeManager;

    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;
    QVBoxLayout* m_contentLayout;

    QLabel* m_titleLabel;
    QLineEdit* m_searchInput;
    QComboBox* m_sortCombo;
    QPushButton* m_importButton;
    QLabel* m_emptyLabel;
    QLabel* m_countLabel;

    QMap<QString, QtTokenCard*> m_tokenCards;
    QList<QString> m_contractAddresses;

    QString m_emptyMessage;
};
