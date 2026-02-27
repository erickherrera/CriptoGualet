#pragma once

#include <QGraphicsOpacityEffect>
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
    void setupTextOpacityEffects();
    void animateTextOpacity(qreal targetOpacity, int duration = 150, int delay = 0);
    void animateHoverLabelOpacity(qreal targetOpacity);
    void stopActiveTextAnimations();
    void createOpacityAnimation(QGraphicsOpacityEffect* effect, qreal targetOpacity, int duration,
                                int delay = 0);

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
    QPropertyAnimation* m_hoverFadeAnimation;
    QList<QPropertyAnimation*> m_activeTextAnimations;
    QMetaObject::Connection m_hoverFadeFinishedConn;

    QLabel* m_hoverLabel;

    QGraphicsOpacityEffect* m_walletTextOpacity;
    QGraphicsOpacityEffect* m_topCryptosTextOpacity;
    QGraphicsOpacityEffect* m_settingsTextOpacity;
    QGraphicsOpacityEffect* m_signOutTextOpacity;

    bool m_isExpanded;
    bool m_textVisible;
    bool m_wasAnimatingExpand;
    Page m_selectedPage;

    static constexpr int COLLAPSED_WIDTH = 90;
    static constexpr int EXPANDED_WIDTH = 310;
    static constexpr int ICON_SIZE = 32;
    static constexpr int BUTTON_HEIGHT = 60;
    static constexpr int TEXT_STAGGER_DELAY = 60;
    static constexpr int TEXT_HIDE_ADVANCE = 30;
};
