#pragma once

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

// Forward declarations
class QtThemeManager;

class QtPasswordConfirmDialog : public QDialog {
  Q_OBJECT

public:
  explicit QtPasswordConfirmDialog(const QString &username,
                                   const QString &title = "Confirm Password",
                                   const QString &message = "Please enter your password to continue:",
                                   QWidget *parent = nullptr);

  // Get the entered password after successful confirmation
  QString getPassword() const { return m_password; }

  // Check if password was confirmed
  bool isConfirmed() const { return m_confirmed; }

private slots:
  void onConfirmClicked();
  void onCancelClicked();

private:
  void setupUI();
  void applyTheme();
  void showError(const QString &error);

  QString m_username;
  QString m_title;
  QString m_message;
  QString m_password;
  bool m_confirmed = false;

  QtThemeManager *m_themeManager;

  QVBoxLayout *m_mainLayout;
  QLabel *m_titleLabel;
  QLabel *m_messageLabel;
  QLineEdit *m_passwordEdit;
  QPushButton *m_confirmButton;
  QPushButton *m_cancelButton;
  QLabel *m_errorLabel;
};
