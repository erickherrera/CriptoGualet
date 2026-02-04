#pragma once

#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class QtThemeManager;

class QtSidebar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int sidebarWidth READ width WRITE setSidebarWidth)

  public:
    enum class Page { Wallet, TopCryptos, Settings, None };
    explicit QtSidebar(QtThemeManager* themeManager, QWidget* parent = nullptr);
    ~QtSidebar() override = default;

    void toggleSidebar();
    void applyTheme();
    void setSelectedPage(Page page);
    void setSidebarWidth(int width);

  signals:
    void navigateToWallet();
    void navigateToSettings();
    void navigateToTopCryptos();
    void signOutRequested();
    void sidebarWidthChanged(int width);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void setupUI();
    void animateSidebar(bool expand);
    void createNavigationButtons();
    QIcon createColoredIcon(const QString& svgPath, const QColor& color);
    void showHoverLabel(const QString& text, int yPos, bool isSignOut = false);
    void hideHoverLabel();
    void updateLabelsVisibility(bool visible);
    void setShadowsEnabled(bool enabled);
    void cacheIcons();

    struct IconPair {
        QPixmap active;
        QPixmap inactive;
    };
    QMap<Page, IconPair> m_iconCache;

    QtThemeManager* m_themeManager;
    QWidget* m_sidebarContent;
    QVBoxLayout* m_mainLayout;
    QVBoxLayout* m_navLayout;

    QPushButton* m_menuButton;
    QPushButton* m_walletButton;
    QPushButton* m_topCryptosButton;
    QPushButton* m_settingsButton;
    QPushButton* m_signOutButton;

    QPropertyAnimation* m_widthAnimation;
    QLabel* m_hoverLabel;

    bool m_isExpanded;
    bool m_textVisible;
    Page m_selectedPage;

    static constexpr int COLLAPSED_WIDTH = 70;
    static constexpr int EXPANDED_WIDTH = 240;
};
