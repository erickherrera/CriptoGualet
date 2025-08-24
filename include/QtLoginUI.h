#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFrame>
#include <QSpacerItem>
#include <QComboBox>
#include <QTimer>

class QtThemeManager;

class QtLoginUI : public QWidget {
    Q_OBJECT

public:
    explicit QtLoginUI(QWidget *parent = nullptr);
    void applyTheme();

signals:
    void loginRequested(const QString &username, const QString &password);
    void registerRequested(const QString &username, const QString &password);

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onThemeChanged();
    void clearMessage();

private:
    void setupUI();
    void setupThemeSelector();
    void createLoginCard();
    void updateStyles();
    void showMessage(const QString &message, bool isError = false);
    
    QtThemeManager *m_themeManager;
    
    QVBoxLayout *m_mainLayout;
    QFrame *m_loginCard;
    QVBoxLayout *m_cardLayout;
    QFormLayout *m_formLayout;
    
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QLabel *m_messageLabel;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QPushButton *m_loginButton;
    QPushButton *m_registerButton;
    QComboBox *m_themeSelector;
    QTimer *m_messageTimer;
    
    QHBoxLayout *m_buttonLayout;
    QHBoxLayout *m_themeLayout;
};