#pragma once

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QPropertyAnimation>

class QtThemeManager;

class QtSidebar : public QWidget {
    Q_OBJECT

public:
    explicit QtSidebar(QtThemeManager *themeManager, QWidget *parent = nullptr);
    ~QtSidebar() = default;

    void toggleSidebar();
    void applyTheme();

signals:
    void navigateToWallet();
    void navigateToSettings();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUI();
    void animateSidebar(bool expand);
    void createNavigationButtons();
    QIcon createColoredIcon(const QString &svgPath, const QColor &color);

    QtThemeManager *m_themeManager;
    QWidget *m_sidebarContent;
    QVBoxLayout *m_mainLayout;
    QVBoxLayout *m_navLayout;

    QPushButton *m_menuButton;
    QPushButton *m_walletButton;
    QPushButton *m_settingsButton;

    QPropertyAnimation *m_widthAnimation;

    bool m_isExpanded;

    static constexpr int COLLAPSED_WIDTH = 70;
    static constexpr int EXPANDED_WIDTH = 240;
};
