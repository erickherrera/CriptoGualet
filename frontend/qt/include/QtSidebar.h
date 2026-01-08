#pragma once

#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class QtThemeManager;

class QtSidebar : public QWidget {
    Q_OBJECT

  public:
    explicit QtSidebar(QtThemeManager* themeManager, QWidget* parent = nullptr);
    ~QtSidebar() override = default;

    void toggleSidebar();
    void applyTheme();

  signals:
    void navigateToWallet();
    void navigateToSettings();
    void navigateToTopCryptos();
    void sidebarWidthChanged(int width);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void setupUI();
    void animateSidebar(bool expand);
    void createNavigationButtons();
    QIcon createColoredIcon(const QString& svgPath, const QColor& color);

    QtThemeManager* m_themeManager;
    QWidget* m_sidebarContent;
    QVBoxLayout* m_mainLayout;
    QVBoxLayout* m_navLayout;

    QPushButton* m_menuButton;
    QPushButton* m_walletButton;
    QPushButton* m_topCryptosButton;
    QPushButton* m_settingsButton;

    QPropertyAnimation* m_widthAnimation;

    bool m_isExpanded;

    static constexpr int COLLAPSED_WIDTH = 70;
    static constexpr int EXPANDED_WIDTH = 240;
};
