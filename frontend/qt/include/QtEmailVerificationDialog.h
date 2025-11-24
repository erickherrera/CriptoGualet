#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

// Forward declarations
class QtThemeManager;

class QtEmailVerificationDialog : public QDialog {
  Q_OBJECT

public:
  explicit QtEmailVerificationDialog(const QString &username,
                                     const QString &email,
                                     QWidget *parent = nullptr);

  // Check if verification was successful
  bool isVerified() const { return m_verified; }

private slots:
  void onVerifyClicked();
  void onResendClicked();
  void onResendCooldownTick();
  void updateResendButton();

private:
  void setupUI();
  void applyTheme();
  void showMessage(const QString &message, bool isError);
  void clearMessage();

  QString m_username;
  QString m_email;
  bool m_verified = false;

  QtThemeManager *m_themeManager;

  QVBoxLayout *m_mainLayout;
  QLabel *m_instructionsLabel;
  QLabel *m_emailLabel;
  QLineEdit *m_codeEdit;
  QPushButton *m_verifyButton;
  QPushButton *m_resendButton;
  QLabel *m_messageLabel;
  QPushButton *m_cancelButton;

  QTimer *m_resendCooldownTimer;
  int m_resendCooldownSeconds = 0;
};
