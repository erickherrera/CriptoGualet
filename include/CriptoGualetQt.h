#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <memory>

class QtLoginUI;
class QtWalletUI;
class QtThemeManager;

class CriptoGualetQt : public QMainWindow {
    Q_OBJECT

public:
    explicit CriptoGualetQt(QWidget *parent = nullptr);
    ~CriptoGualetQt() = default;

public slots:
    void showLoginScreen();
    void showWalletScreen();
    void onThemeChanged();

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void createNavbar();
    void updateNavbarVisibility();
    void applyNavbarStyling();
    
    QWidget *m_centralWidget;
    QVBoxLayout *m_mainLayout;
    QFrame *m_navbar;
    QLabel *m_appTitleLabel;
    QPushButton *m_signOutButton;
    QStackedWidget *m_stackedWidget;
    QtLoginUI *m_loginUI;
    QtWalletUI *m_walletUI;
    QtThemeManager *m_themeManager;
};