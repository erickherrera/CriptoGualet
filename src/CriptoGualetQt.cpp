#include "../include/CriptoGualetQt.h"
#include "../include/QtLoginUI.h"
#include "../include/QtWalletUI.h"
#include "../include/QtThemeManager.h"
#include "../include/CriptoGualet.h"
#include "../include/Auth.h"

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QMessageBox>
#include <QClipboard>
#include <QStackedWidget>
#include <QMenu>
#include <QAction>
#include <map>
#include <string>

extern std::map<std::string, User> g_users;
extern std::string g_currentUser;

CriptoGualetQt::CriptoGualetQt(QWidget *parent)
    : QMainWindow(parent)
    , m_stackedWidget(nullptr)
    , m_loginUI(nullptr)
    , m_walletUI(nullptr)
    , m_themeManager(&QtThemeManager::instance())
{
    setWindowTitle("CriptoGualet - Secure Bitcoin Wallet");
    setMinimumSize(800, 600);
    resize(1000, 700);
    
    setupUI();
    setupMenuBar();
    setupStatusBar();
    
    connect(m_themeManager, &QtThemeManager::themeChanged, this, &CriptoGualetQt::onThemeChanged);
    
    showLoginScreen();
}

void CriptoGualetQt::setupUI() {
    m_stackedWidget = new QStackedWidget(this);
    setCentralWidget(m_stackedWidget);
    
    m_loginUI = new QtLoginUI(this);
    m_walletUI = new QtWalletUI(this);
    
    m_stackedWidget->addWidget(m_loginUI);
    m_stackedWidget->addWidget(m_walletUI);
    
    connect(m_loginUI, &QtLoginUI::loginRequested, [this](const QString &username, const QString &password) {
        std::string stdUsername = username.toStdString();
        std::string stdPassword = password.toStdString();
        
        if (Auth::LoginUser(stdUsername, stdPassword)) {
            g_currentUser = stdUsername;
            m_walletUI->setUserInfo(username, QString::fromStdString(g_users[stdUsername].walletAddress));
            showWalletScreen();
            statusBar()->showMessage("Login successful", 3000);
        } else {
            QMessageBox::warning(this, "Login Failed", "User not found.");
        }
    });
    
    connect(m_loginUI, &QtLoginUI::registerRequested, [this](const QString &username, const QString &password) {
        std::string stdUsername = username.toStdString();
        std::string stdPassword = password.toStdString();
        
        if (Auth::RegisterUser(stdUsername, stdPassword)) {
            QMessageBox::information(this, "Registration Successful", 
                QString("Account created for %1!\nYour Bitcoin address: %2")
                .arg(username)
                .arg(QString::fromStdString(g_users[stdUsername].walletAddress)));
        } else {
            QMessageBox::warning(this, "Registration Failed", "Username already exists.");
        }
    });
    
    connect(m_walletUI, &QtWalletUI::logoutRequested, this, &CriptoGualetQt::showLoginScreen);
    
    connect(m_walletUI, &QtWalletUI::viewBalanceRequested, [this]() {
        QMessageBox::information(this, "Balance", "Balance: 0.00000000 BTC\n(Demo wallet - no real transactions)");
    });
    
    connect(m_walletUI, &QtWalletUI::sendBitcoinRequested, [this]() {
        QMessageBox::information(this, "Send Bitcoin", "Send functionality would be implemented here.\n(Demo wallet)");
    });
    
    connect(m_walletUI, &QtWalletUI::receiveBitcoinRequested, [this]() {
        if (!g_currentUser.empty() && g_users.find(g_currentUser) != g_users.end()) {
            QString address = QString::fromStdString(g_users[g_currentUser].walletAddress);
            QApplication::clipboard()->setText(address);
            QMessageBox::information(this, "Receive Bitcoin", 
                QString("Your Bitcoin address has been copied to clipboard:\n%1").arg(address));
        }
    });
}

void CriptoGualetQt::setupMenuBar() {
    QMenu *themeMenu = menuBar()->addMenu("&Theme");
    
    QAction *darkAction = themeMenu->addAction("Dark Theme");
    QAction *lightAction = themeMenu->addAction("Light Theme");
    QAction *cryptoDarkAction = themeMenu->addAction("Crypto Dark");
    QAction *cryptoLightAction = themeMenu->addAction("Crypto Light");
    
    connect(darkAction, &QAction::triggered, [this]() {
        m_themeManager->applyTheme(ThemeType::DARK);
    });
    
    connect(lightAction, &QAction::triggered, [this]() {
        m_themeManager->applyTheme(ThemeType::LIGHT);
    });
    
    connect(cryptoDarkAction, &QAction::triggered, [this]() {
        m_themeManager->applyTheme(ThemeType::CRYPTO_DARK);
    });
    
    connect(cryptoLightAction, &QAction::triggered, [this]() {
        m_themeManager->applyTheme(ThemeType::CRYPTO_LIGHT);
    });
    
    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *aboutAction = helpMenu->addAction("&About");
    
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "About CriptoGualet", 
            "CriptoGualet v1.0\n\nA secure Bitcoin wallet application built with Qt.\n\n"
            "Features:\n• Modern Qt UI with theming\n• Secure authentication\n• Bitcoin address generation\n• Demo wallet functionality");
    });
}

void CriptoGualetQt::setupStatusBar() {
    statusBar()->showMessage("Ready");
}

void CriptoGualetQt::showLoginScreen() {
    g_currentUser.clear();
    m_stackedWidget->setCurrentWidget(m_loginUI);
    statusBar()->showMessage("Please log in or create an account");
}

void CriptoGualetQt::showWalletScreen() {
    m_stackedWidget->setCurrentWidget(m_walletUI);
    statusBar()->showMessage(QString("Logged in as: %1").arg(QString::fromStdString(g_currentUser)));
}

void CriptoGualetQt::onThemeChanged() {
    m_loginUI->applyTheme();
    m_walletUI->applyTheme();
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

