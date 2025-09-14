#include "../include/CriptoGualetQt.h"
#include "../include/Auth.h"
#include "../include/CriptoGualet.h"
#include "../include/QtLoginUI.h"
#include "../include/QtThemeManager.h"
#include "../include/QtWalletUI.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <map>
#include <string>

extern std::map<std::string, User> g_users;
extern std::string g_currentUser;

CriptoGualetQt::CriptoGualetQt(QWidget *parent)
    : QMainWindow(parent), m_stackedWidget(nullptr), m_loginUI(nullptr),
      m_walletUI(nullptr), m_themeManager(&QtThemeManager::instance()) {
  setWindowTitle("CriptoGualet - Secure Bitcoin Wallet");
  setMinimumSize(800, 600);
  resize(1000, 700);

  setupUI();
  setupMenuBar();
  setupStatusBar();

  // Apply initial styling
  applyNavbarStyling();

  connect(m_themeManager, &QtThemeManager::themeChanged, this,
          &CriptoGualetQt::onThemeChanged);

  showLoginScreen();
}

void CriptoGualetQt::setupUI() {
  // Create central widget with main layout
  m_centralWidget = new QWidget(this);
  setCentralWidget(m_centralWidget);

  m_mainLayout = new QVBoxLayout(m_centralWidget);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(0);

  // Create navbar
  createNavbar();

  // Create stacked widget for pages
  m_stackedWidget = new QStackedWidget(m_centralWidget);
  m_mainLayout->addWidget(m_stackedWidget);

  m_loginUI = new QtLoginUI(this);
  m_walletUI = new QtWalletUI(this);

  m_stackedWidget->addWidget(m_loginUI);
  m_stackedWidget->addWidget(m_walletUI);

  connect(m_loginUI, &QtLoginUI::loginRequested,
          [this](const QString &username, const QString &password) {
            std::string stdUsername = username.toStdString();
            std::string stdPassword = password.toStdString();

            Auth::AuthResponse response =
                Auth::LoginUser(stdUsername, stdPassword);
            QString message = QString::fromStdString(response.message);

            if (response.success()) {
              g_currentUser = stdUsername;
              m_walletUI->setUserInfo(
                  username,
                  QString::fromStdString(g_users[stdUsername].walletAddress));
              showWalletScreen();
              statusBar()->showMessage("Login successful", 3000);
            } else {
              statusBar()->showMessage("Login failed", 3000);
            }

            // Send result back to login UI for visual feedback
            m_loginUI->onLoginResult(response.success(), message);
          });

  connect(m_loginUI, &QtLoginUI::registerRequested,
          [this](const QString &username, const QString &password) {
            std::string stdUsername = username.toStdString();
            std::string stdPassword = password.toStdString();

            // Debug output
            qDebug() << "Registration attempt - Username:" << username
                     << "Password length:" << password.length();

            Auth::AuthResponse response =
                Auth::RegisterUser(stdUsername, stdPassword);
            QString message = QString::fromStdString(response.message);

            // Debug output
            qDebug() << "Registration response - Success:" << response.success()
                     << "Message:" << message;
            qDebug() << "Auth result code:"
                     << static_cast<int>(response.result);

            if (response.success()) {
              statusBar()->showMessage("Registration successful", 3000);
              // Show additional info in popup for successful registration
              QMessageBox::information(
                  this, "Registration Successful",
                  QString("Account created for %1!\n\nYou can now sign in with your credentials.")
                      .arg(username));
            } else {
              statusBar()->showMessage("Registration failed", 3000);
              qDebug() << "Registration failed with message:" << message;
            }

            // Send result back to login UI for visual feedback
            m_loginUI->onRegisterResult(response.success(), message);
          });

  // Logout handled by navbar sign out button

  connect(m_walletUI, &QtWalletUI::viewBalanceRequested, [this]() {
    QMessageBox::information(
        this, "Balance",
        "Balance: 0.00000000 BTC\n(Demo wallet - no real transactions)");
  });

  connect(m_walletUI, &QtWalletUI::sendBitcoinRequested, [this]() {
    QMessageBox::information(
        this, "Send Bitcoin",
        "Send functionality would be implemented here.\n(Demo wallet)");
  });

  connect(m_walletUI, &QtWalletUI::receiveBitcoinRequested, [this]() {
    if (!g_currentUser.empty() &&
        g_users.find(g_currentUser) != g_users.end()) {
      QString address =
          QString::fromStdString(g_users[g_currentUser].walletAddress);
      QApplication::clipboard()->setText(address);
      QMessageBox::information(
          this, "Receive Bitcoin",
          QString("Your Bitcoin address has been copied to clipboard:\n%1")
              .arg(address));
    }
  });
}

void CriptoGualetQt::createNavbar() {
  m_navbar = new QFrame(m_centralWidget);
  m_navbar->setProperty("class", "navbar");
  m_navbar->setFixedHeight(60);

  QHBoxLayout *navLayout = new QHBoxLayout(m_navbar);
  navLayout->setContentsMargins(20, 10, 20, 10);
  navLayout->setSpacing(10);

  // App title/logo
  m_appTitleLabel = new QLabel("CriptoGualet", m_navbar);
  m_appTitleLabel->setProperty("class", "navbar-title");
  navLayout->addWidget(m_appTitleLabel);

  // Spacer to push sign out button to the right
  navLayout->addStretch();

  // Sign out button
  m_signOutButton = new QPushButton("Sign Out", m_navbar);
  m_signOutButton->setProperty("class", "navbar-button");
  m_signOutButton->setMaximumWidth(100);
  navLayout->addWidget(m_signOutButton);

  // Connect sign out button
  connect(m_signOutButton, &QPushButton::clicked, this,
          &CriptoGualetQt::showLoginScreen);

  // Add navbar to main layout (at the top)
  m_mainLayout->insertWidget(0, m_navbar);

  // Initially hidden until user logs in
  m_navbar->hide();
}

void CriptoGualetQt::setupMenuBar() {
  QMenu *themeMenu = menuBar()->addMenu("&Theme");

  QAction *darkAction = themeMenu->addAction("Dark Theme");
  QAction *lightAction = themeMenu->addAction("Light Theme");
  QAction *cryptoDarkAction = themeMenu->addAction("Crypto Dark");
  QAction *cryptoLightAction = themeMenu->addAction("Crypto Light");

  connect(darkAction, &QAction::triggered,
          [this]() { m_themeManager->applyTheme(ThemeType::DARK); });

  connect(lightAction, &QAction::triggered,
          [this]() { m_themeManager->applyTheme(ThemeType::LIGHT); });

  connect(cryptoDarkAction, &QAction::triggered,
          [this]() { m_themeManager->applyTheme(ThemeType::CRYPTO_DARK); });

  connect(cryptoLightAction, &QAction::triggered,
          [this]() { m_themeManager->applyTheme(ThemeType::CRYPTO_LIGHT); });

  QMenu *helpMenu = menuBar()->addMenu("&Help");
  QAction *aboutAction = helpMenu->addAction("&About");

  connect(aboutAction, &QAction::triggered, [this]() {
    QMessageBox::about(
        this, "About CriptoGualet",
        "CriptoGualet v1.0\n\nA secure Bitcoin wallet application built with "
        "Qt.\n\n"
        "Features:\n• Modern Qt UI with theming\n• Secure authentication\n• "
        "Bitcoin address generation\n• Demo wallet functionality");
  });
}

void CriptoGualetQt::setupStatusBar() { statusBar()->showMessage("Ready"); }

void CriptoGualetQt::showLoginScreen() {
  g_currentUser.clear();
  m_stackedWidget->setCurrentWidget(m_loginUI);
  updateNavbarVisibility();
  statusBar()->showMessage("Please log in or create an account");
}

void CriptoGualetQt::showWalletScreen() {
  m_stackedWidget->setCurrentWidget(m_walletUI);
  updateNavbarVisibility();
  statusBar()->showMessage(
      QString("Logged in as: %1").arg(QString::fromStdString(g_currentUser)));
}

void CriptoGualetQt::updateNavbarVisibility() {
  // Show navbar only when wallet screen is visible
  if (m_stackedWidget->currentWidget() == m_walletUI) {
    m_navbar->show();
  } else {
    m_navbar->hide();
  }
}

void CriptoGualetQt::onThemeChanged() {
  m_loginUI->applyTheme();
  m_walletUI->applyTheme();
  applyNavbarStyling();
}

void CriptoGualetQt::applyNavbarStyling() {
  // Apply main window stylesheet (includes navbar styles)
  setStyleSheet(m_themeManager->getMainWindowStyleSheet());

  // Apply fonts to navbar components
  m_appTitleLabel->setFont(m_themeManager->titleFont());
  m_signOutButton->setFont(m_themeManager->buttonFont());
}

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  app.setApplicationName("CriptoGualet");
  app.setApplicationVersion("1.0");
  app.setOrganizationName("CriptoGualet");

  CriptoGualetQt window;
  window.show();

  return app.exec();
}
