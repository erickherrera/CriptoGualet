#include "QtEmailVerificationDialog.h"
#include "Auth.h"
#include "QtThemeManager.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

QtEmailVerificationDialog::QtEmailVerificationDialog(const QString &username,
                                                     const QString &email,
                                                     QWidget *parent)
    : QDialog(parent), m_username(username), m_email(email),
      m_themeManager(&QtThemeManager::instance()) {
  setWindowTitle("Email Verification");
  setModal(true);
  setMinimumSize(450, 300);
  resize(450, 300);

  m_resendCooldownTimer = new QTimer(this);
  m_resendCooldownTimer->setInterval(1000); // 1 second
  connect(m_resendCooldownTimer, &QTimer::timeout, this,
          &QtEmailVerificationDialog::onResendCooldownTick);

  setupUI();
  applyTheme();
}

void QtEmailVerificationDialog::setupUI() {
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setContentsMargins(30, 30, 30, 30);
  m_mainLayout->setSpacing(15);

  // Title and instructions
  QLabel *titleLabel = new QLabel("Verify Your Email", this);
  titleLabel->setProperty("class", "title");
  QFont titleFont = m_themeManager->titleFont();
  titleFont.setPointSizeF(titleFont.pointSizeF() + 2);
  titleLabel->setFont(titleFont);
  titleLabel->setAlignment(Qt::AlignCenter);

  m_instructionsLabel = new QLabel(
      "We've sent a 6-digit verification code to your email address.\n"
      "Please enter the code below to verify your account.",
      this);
  m_instructionsLabel->setWordWrap(true);
  m_instructionsLabel->setAlignment(Qt::AlignCenter);
  m_instructionsLabel->setFont(m_themeManager->textFont());

  m_emailLabel = new QLabel("Email: " + m_email, this);
  m_emailLabel->setAlignment(Qt::AlignCenter);
  m_emailLabel->setFont(m_themeManager->textFont());
  m_emailLabel->setStyleSheet(
      QString("color: %1; font-weight: 600;")
          .arg(m_themeManager->accentColor().name()));

  // Code input field
  m_codeEdit = new QLineEdit(this);
  m_codeEdit->setPlaceholderText("Enter 6-digit code");
  m_codeEdit->setMaxLength(6);
  m_codeEdit->setAlignment(Qt::AlignCenter);
  m_codeEdit->setMinimumHeight(50);
  m_codeEdit->setFont(m_themeManager->textFont());

  // Only allow digits
  QRegularExpressionValidator *validator = new QRegularExpressionValidator(
      QRegularExpression("\\d{0,6}"), m_codeEdit);
  m_codeEdit->setValidator(validator);

  // Verify button
  m_verifyButton = new QPushButton("Verify Email", this);
  m_verifyButton->setMinimumHeight(44);
  m_verifyButton->setFont(m_themeManager->buttonFont());

  // Resend button
  m_resendButton = new QPushButton("Resend Code", this);
  m_resendButton->setMinimumHeight(36);
  m_resendButton->setFont(m_themeManager->buttonFont());

  // Message label (for errors/success)
  m_messageLabel = new QLabel(this);
  m_messageLabel->setAlignment(Qt::AlignCenter);
  m_messageLabel->setWordWrap(true);
  m_messageLabel->setMinimumHeight(40);
  m_messageLabel->hide();

  // Cancel button
  m_cancelButton = new QPushButton("Cancel", this);
  m_cancelButton->setMinimumHeight(36);
  m_cancelButton->setFont(m_themeManager->buttonFont());

  // Layout
  m_mainLayout->addWidget(titleLabel);
  m_mainLayout->addSpacing(10);
  m_mainLayout->addWidget(m_instructionsLabel);
  m_mainLayout->addWidget(m_emailLabel);
  m_mainLayout->addSpacing(10);
  m_mainLayout->addWidget(m_codeEdit);
  m_mainLayout->addWidget(m_verifyButton);
  m_mainLayout->addSpacing(5);
  m_mainLayout->addWidget(m_resendButton);
  m_mainLayout->addWidget(m_messageLabel);
  m_mainLayout->addStretch();
  m_mainLayout->addWidget(m_cancelButton);

  // Connect signals
  connect(m_verifyButton, &QPushButton::clicked, this,
          &QtEmailVerificationDialog::onVerifyClicked);
  connect(m_resendButton, &QPushButton::clicked, this,
          &QtEmailVerificationDialog::onResendClicked);
  connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_codeEdit, &QLineEdit::returnPressed, this,
          &QtEmailVerificationDialog::onVerifyClicked);
}

void QtEmailVerificationDialog::applyTheme() {
  // Dialog background
  QPalette pal = palette();
  pal.setColor(QPalette::Window, m_themeManager->backgroundColor());
  setPalette(pal);
  setAutoFillBackground(true);

  // Code input field styling
  QString inputBg = m_themeManager->surfaceColor().name();
  QString textHex = m_themeManager->textColor().name();
  QString borderColor = m_themeManager->secondaryColor().name();
  QString accentHex = m_themeManager->accentColor().name();

  QString codeEditStyle = QString(R"(
        QLineEdit {
            background-color: %1;
            color: %2;
            border: 2px solid %3;
            border-radius: 8px;
            padding: 10px;
            font-size: 24px;
            font-weight: 600;
            letter-spacing: 8px;
        }
        QLineEdit:focus {
            border: 2px solid %4;
        }
    )")
                              .arg(inputBg, textHex, borderColor, accentHex);

  m_codeEdit->setStyleSheet(codeEditStyle);

  // Verify button (primary)
  QString whiteText = QColor(Qt::white).name();
  QString verifyButtonStyle = QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: %3;
        }
        QPushButton:pressed {
            background-color: %4;
        }
        QPushButton:disabled {
            background-color: %5;
            color: %6;
        }
    )")
                                  .arg(accentHex, whiteText,
                                       m_themeManager->accentColor().lighter(110).name(),
                                       m_themeManager->accentColor().darker(110).name(),
                                       m_themeManager->secondaryColor().name(),
                                       m_themeManager->subtitleColor().name());

  m_verifyButton->setStyleSheet(verifyButtonStyle);

  // Resend and Cancel buttons (secondary)
  QString secondaryButtonStyle = QString(R"(
        QPushButton {
            background-color: transparent;
            color: %1;
            border: 2px solid %1;
            border-radius: 8px;
            font-size: 13px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: %2;
            border-color: %3;
        }
        QPushButton:pressed {
            background-color: %4;
        }
        QPushButton:disabled {
            color: %5;
            border-color: %5;
        }
    )")
                                     .arg(accentHex,
                                          m_themeManager->accentColor().lighter(180).name(),
                                          m_themeManager->accentColor().lighter(120).name(),
                                          m_themeManager->accentColor().lighter(160).name(),
                                          m_themeManager->subtitleColor().name());

  m_resendButton->setStyleSheet(secondaryButtonStyle);
  m_cancelButton->setStyleSheet(secondaryButtonStyle);
}

void QtEmailVerificationDialog::onVerifyClicked() {
  QString code = m_codeEdit->text().trimmed();

  clearMessage();

  if (code.length() != 6) {
    showMessage("Please enter a 6-digit code", true);
    return;
  }

  // Call Auth::VerifyEmailCode
  auto result = Auth::VerifyEmailCode(m_username.toStdString(), code.toStdString());

  if (result.result == Auth::AuthResult::SUCCESS) {
    m_verified = true;
    showMessage(QString::fromStdString(result.message), false);

    // Success! Close dialog after 1 second
    QTimer::singleShot(1000, this, &QDialog::accept);
  } else {
    showMessage(QString::fromStdString(result.message), true);
  }
}

void QtEmailVerificationDialog::onResendClicked() {
  clearMessage();

  // Disable button during request
  m_resendButton->setEnabled(false);
  m_resendButton->setText("Sending...");

  // Call Auth::ResendVerificationCode
  auto result =
      Auth::ResendVerificationCode(m_username.toStdString());

  if (result.result == Auth::AuthResult::SUCCESS) {
    showMessage(QString::fromStdString(result.message), false);

    // Start 60-second cooldown
    m_resendCooldownSeconds = 60;
    m_resendCooldownTimer->start();
    updateResendButton();
  } else if (result.result == Auth::AuthResult::RATE_LIMITED) {
    showMessage(QString::fromStdString(result.message), true);
    m_resendButton->setEnabled(true);
    m_resendButton->setText("Resend Code");
  } else {
    showMessage(QString::fromStdString(result.message), true);
    m_resendButton->setEnabled(true);
    m_resendButton->setText("Resend Code");
  }
}

void QtEmailVerificationDialog::onResendCooldownTick() {
  m_resendCooldownSeconds--;

  if (m_resendCooldownSeconds <= 0) {
    m_resendCooldownTimer->stop();
    m_resendButton->setEnabled(true);
    m_resendButton->setText("Resend Code");
  } else {
    updateResendButton();
  }
}

void QtEmailVerificationDialog::updateResendButton() {
  if (m_resendCooldownSeconds > 0) {
    m_resendButton->setText(
        QString("Resend in %1s").arg(m_resendCooldownSeconds));
    m_resendButton->setEnabled(false);
  } else {
    m_resendButton->setText("Resend Code");
    m_resendButton->setEnabled(true);
  }
}

void QtEmailVerificationDialog::showMessage(const QString &message,
                                            bool isError) {
  if (m_messageLabel) {
    m_messageLabel->setText(message);
    m_messageLabel->setProperty("isError", isError);
    m_messageLabel->setStyleSheet(
        isError ? m_themeManager->getErrorMessageStyleSheet()
                : m_themeManager->getSuccessMessageStyleSheet());
    m_messageLabel->show();
  }
}

void QtEmailVerificationDialog::clearMessage() {
  if (m_messageLabel) {
    m_messageLabel->clear();
    m_messageLabel->hide();
  }
}
