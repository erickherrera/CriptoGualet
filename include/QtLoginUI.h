#pragma once

#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpacerItem>
#include <QTimer>
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
  explicit QtLoginUI(QWidget *parent = nullptr);
  void applyTheme();

signals:
  void loginRequested(const QString &username, const QString &password);
  void registerRequested(const QString &username, const QString &email, const QString &password);

public slots:
  void onLoginResult(bool success, const QString &message);
  void onRegisterResult(bool success, const QString &message);

private slots:
  void onLoginClicked();
  void onRegisterClicked();
  void onRegisterModeToggled(bool registerMode);
  void onThemeChanged();
  void clearMessage();
  void onPasswordVisibilityToggled();
  void onRevealSeedClicked();
  void onRestoreSeedClicked();

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
  QLineEdit *m_emailEdit;
  QLineEdit *m_passwordEdit;
  QPushButton *m_passwordToggleButton;
  QPushButton *m_loginButton;
  QPushButton *m_registerButton;
  QPushButton *m_revealSeedButton;
  QPushButton *m_restoreSeedButton;
  QComboBox *m_themeSelector;
  QTimer *m_messageTimer;

  QHBoxLayout *m_buttonLayout;
  QHBoxLayout *m_themeLayout;
};
