#pragma once

#include "../../../backend/core/include/WalletAPI.h"
#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <memory>

class QtLoginUI;
class QtWalletUI;
class QtSettingsUI;
class QtTopCryptosPage;
class QtThemeManager;
class QtSidebar;

class CriptoGualetQt : public QMainWindow {
  Q_OBJECT

public:
  explicit CriptoGualetQt(QWidget *parent = nullptr);
  ~CriptoGualetQt() = default;

public slots:
  void showLoginScreen();
  void showWalletScreen();
  void showSettingsScreen();
  void showTopCryptosPage();
  void onThemeChanged();

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void setupUI();
  void setupMenuBar();
  void setupStatusBar();
  void createNavbar();
  void createSidebar();
  void updateNavbarVisibility();
  void updateSidebarVisibility();
  void applyNavbarStyling();

  QWidget *m_centralWidget;
  QVBoxLayout *m_mainLayout;
  QFrame *m_navbar;
  QLabel *m_appTitleLabel;
  QPushButton *m_signOutButton;
  QStackedWidget *m_stackedWidget;
  QtLoginUI *m_loginUI;
  QtWalletUI *m_walletUI;
  QtSettingsUI *m_settingsUI;
  QtTopCryptosPage *m_topCryptosPage;
  QtSidebar *m_sidebar;
  QtThemeManager *m_themeManager;
  std::unique_ptr<WalletAPI::SimpleWallet> m_wallet;
};
