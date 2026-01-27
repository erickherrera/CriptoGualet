#pragma once

#include <QTabBar>
#include <QTimer>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

// Forward declarations
class QtThemeManager;
class QDialog;
class QDialogButtonBox;
class QCheckBox;
class QPlainTextEdit;
class QGridLayout;
class QMessageBox;

class QtLoginUI : public QWidget {
    Q_OBJECT

  public:
    explicit QtLoginUI(QWidget* parent = nullptr);
    void applyTheme();
    void clearLoginFields();
    void setAuthInProgress(bool inProgress);

  signals:
    void loginRequested(const QString& username, const QString& password);
    void registerRequested(const QString& username, const QString& password);
    void totpVerificationRequired(const QString& username, const QString& password,
                                  const QString& totpCode);
    void loginSuccessful(const QString& sessionId);

  public slots:
    void onLoginResult(bool success, const QString& message);
    void onRegisterResult(bool success, const QString& message);

  private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onRegisterModeToggled(bool registerMode);
    void onThemeChanged();
    void clearMessage();
    void onPasswordVisibilityToggled();
    void onRevealSeedClicked();
    void onRestoreSeedClicked();
    void validateRegisterForm();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void setupUI();
    void createLoginCard();
    void updateStyles();
    void showMessage(const QString& message, bool isError = false);

    QtThemeManager* m_themeManager;

    QVBoxLayout* m_mainLayout;
    QFrame* m_loginCard;
    QVBoxLayout* m_cardLayout;
    QFormLayout* m_formLayout;
    QTabBar* m_tabBar;
    QStackedWidget* m_stackedWidget;

    QLabel* m_titleLabel;
    QLabel* m_subtitleLabel;
    QLabel* m_messageLabel;

    // Sign In tab widgets
    QLineEdit* m_loginUsernameEdit;
    QLineEdit* m_loginPasswordEdit;
    QPushButton* m_loginPasswordToggleButton;
    QPushButton* m_loginButton;

    // Register tab widgets
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_confirmPasswordEdit;
    QPushButton* m_passwordToggleButton;
    QPushButton* m_registerButton;

    // Shared buttons
    QPushButton* m_revealSeedButton;
    QPushButton* m_restoreSeedButton;
    QTimer* m_messageTimer;

    QHBoxLayout* m_buttonLayout;
};
