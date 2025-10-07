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
#include <memory>
#include "PriceService.h"

class QtThemeManager;

// Card widget for displaying individual cryptocurrency data
class QtCryptoCard : public QFrame {
    Q_OBJECT

public:
    explicit QtCryptoCard(QWidget *parent = nullptr);
    void setCryptoData(const PriceService::CryptoPriceData &data, int rank);
    void applyTheme();

private:
    void setupUI();
    QString formatPrice(double price);
    QString formatMarketCap(double marketCap);

    QLabel *m_rankLabel;
    QLabel *m_symbolLabel;
    QLabel *m_nameLabel;
    QLabel *m_priceLabel;
    QLabel *m_changeLabel;
    QLabel *m_marketCapLabel;

    QtThemeManager *m_themeManager;
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

private:
    void setupUI();
    void createHeader();
    void createCryptoCards();
    void fetchTopCryptos();
    void updateCards();
    void updateScrollAreaWidth();

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
    QPushButton *m_refreshButton;
    QPushButton *m_backButton;

    // Crypto cards
    QVector<QtCryptoCard*> m_cryptoCards;
    QWidget *m_cardsContainer;
    QVBoxLayout *m_cardsLayout;

    // Price service
    std::unique_ptr<PriceService::PriceFetcher> m_priceFetcher;
    std::vector<PriceService::CryptoPriceData> m_cryptoData;

    // Auto-refresh timer
    QTimer *m_refreshTimer;
};
