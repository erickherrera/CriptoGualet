#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>
#include <QTimer>
#include <QPushButton>
#include <QResizeEvent>
#include <QNetworkAccessManager>
#include <QMap>
#include <QLineEdit>
#include <QComboBox>
#include <QFutureWatcher>
#include <QProgressBar>
#include <QDateTime>
#include <memory>
#include "../../../backend/blockchain/include/PriceService.h"

class QtThemeManager;

// Card widget for displaying individual cryptocurrency data
class QtCryptoCard : public QFrame {
    Q_OBJECT

public:
    explicit QtCryptoCard(QWidget *parent = nullptr);
    void setCryptoData(const PriceService::CryptoPriceData &cryptoData, int rank);
    void applyTheme();
    void loadIcon(const QString &symbol);
    bool isIconLoaded() const { return m_iconLoaded; }

private:
    void setupUI();
    QString formatPrice(double price);
    QString formatMarketCap(double marketCap);
    QString getCryptoIconUrl(const QString &symbol);
    void setFallbackIcon();

    QLabel *m_iconLabel;
    QLabel *m_symbolLabel;
    QLabel *m_nameLabel;
    QLabel *m_priceLabel;
    QLabel *m_changeLabel;
    QLabel *m_marketCapLabel;

    QtThemeManager *m_themeManager;
    QNetworkAccessManager *m_networkManager;
    QString m_currentSymbol;
    QString m_currentImageUrl;
    bool m_iconLoaded;

private slots:
    void onIconDownloaded(QNetworkReply *reply);
};

// Main page for displaying top cryptocurrencies
class QtTopCryptosPage : public QWidget {
    Q_OBJECT

public:
    explicit QtTopCryptosPage(QWidget *parent = nullptr);
    void applyTheme();
    void refreshData();

signals:
    void backRequested();

protected:
    void resizeEvent(QResizeEvent *event) override;

  private slots:
    void onRefreshClicked();
    void onBackClicked();
    void onAutoRefreshTimer();
    void onSearchTextChanged(const QString &text);
    void onSortChanged(int index);
    void onSearchDebounceTimer();
    void onTopCryptosFetched();
    void onRetryStatusTimer();


private:
    void setupUI();
    void createHeader();
    void createSearchBar();
    void createSortDropdown();
    void createCryptoCards();
    void fetchTopCryptos();
    void updateCards();
    void updateScrollAreaWidth();
    void filterAndSortData();
    void applySearchFilter();
    void applySorting();
    void updateResultCounter();
    void loadVisibleIcons();

    QtThemeManager *m_themeManager;

    // Layout
    QVBoxLayout *m_mainLayout;
    QScrollArea *m_scrollArea;
    QWidget *m_scrollContent;
    QVBoxLayout *m_contentLayout;

    // Header
    QWidget *m_headerWidget;
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QProgressBar *m_loadingBar;
    QPushButton *m_refreshButton;
    QPushButton *m_backButton;

    // Search and filter controls
    QLineEdit *m_searchBox;
    QPushButton *m_clearSearchButton;
    QComboBox *m_sortDropdown;
    QLabel *m_resultCounterLabel;

    // Crypto cards
    QVector<QtCryptoCard*> m_cryptoCards;
    QWidget *m_cardsContainer;
    QVBoxLayout *m_cardsLayout;

    // Price service
    std::unique_ptr<PriceService::PriceFetcher> m_priceFetcher;
    QFutureWatcher<std::vector<PriceService::CryptoPriceData>>* m_topCryptosWatcher;
    std::vector<PriceService::CryptoPriceData> m_cryptoData;
    std::vector<PriceService::CryptoPriceData> m_filteredData;

    // Search and sort state
    QString m_searchText;
    int m_currentSortIndex;
    QTimer *m_searchDebounceTimer;

    // Auto-refresh timer
    QTimer *m_refreshTimer;

    // Retry status timer
    QTimer *m_retryStatusTimer;
    int m_retryStatusAttempt;
    int m_retryStatusMaxAttempts;

    QDateTime m_lastUpdated;
};
