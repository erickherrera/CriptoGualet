#include "QtPasswordConfirmDialog.h"
#include "../../backend/core/include/Auth.h"
#include "QtThemeManager.h"

#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

QtPasswordConfirmDialog::~QtPasswordConfirmDialog() {
    // SECURITY: Securely wipe the password from memory before destruction.
    // QString uses copy-on-write semantics, so we overwrite the internal data.
    if (!m_password.isEmpty()) {
        m_password.fill(QChar('\0'));
        m_password.clear();
    }
    // Also clear the line edit's internal buffer
    if (m_passwordEdit) {
        m_passwordEdit->clear();
    }
}

QtPasswordConfirmDialog::QtPasswordConfirmDialog(const QString& username, const QString& title,
                                                 const QString& message, QWidget* parent)
    : QDialog(parent),
      m_username(username),
      m_title(title),
      m_message(message),
      m_themeManager(&QtThemeManager::instance()) {
    setWindowTitle(title);
    setModal(true);
    setMinimumWidth(450);

    setupUI();
    applyTheme();

    // Connect to theme changes
    connect(m_themeManager, &QtThemeManager::themeChanged, this,
            &QtPasswordConfirmDialog::applyTheme);
}

void QtPasswordConfirmDialog::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(30, 30, 30, 30);
    m_mainLayout->setSpacing(20);

    // Title
    m_titleLabel = new QLabel(m_title, this);
    m_titleLabel->setProperty("class", "dialog-title");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_themeManager->titleFont();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_mainLayout->addWidget(m_titleLabel);

    // Message
    m_messageLabel = new QLabel(m_message, this);
    m_messageLabel->setProperty("class", "dialog-message");
    m_messageLabel->setAlignment(Qt::AlignLeft);
    m_messageLabel->setWordWrap(true);
    QFont messageFont = m_themeManager->textFont();
    messageFont.setPointSize(12);
    m_messageLabel->setFont(messageFont);
    m_mainLayout->addWidget(m_messageLabel);

    // Password input
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText("Enter your password");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setMinimumHeight(40);
    QFont inputFont = m_themeManager->textFont();
    inputFont.setPointSize(12);
    m_passwordEdit->setFont(inputFont);
    m_mainLayout->addWidget(m_passwordEdit);

    // Error label (hidden by default)
    m_errorLabel = new QLabel(this);
    m_errorLabel->setProperty("class", "error-label");
    m_errorLabel->setAlignment(Qt::AlignLeft);
    m_errorLabel->setWordWrap(true);
    QFont errorFont = m_themeManager->textFont();
    errorFont.setPointSize(11);
    m_errorLabel->setFont(errorFont);
    m_errorLabel->hide();
    m_mainLayout->addWidget(m_errorLabel);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setProperty("class", "secondary-button");
    m_cancelButton->setMinimumHeight(40);
    m_cancelButton->setMinimumWidth(120);
    buttonLayout->addWidget(m_cancelButton);

    buttonLayout->addStretch();

    m_confirmButton = new QPushButton("Confirm", this);
    m_confirmButton->setProperty("class", "primary-button");
    m_confirmButton->setMinimumHeight(40);
    m_confirmButton->setMinimumWidth(120);
    buttonLayout->addWidget(m_confirmButton);

    m_mainLayout->addLayout(buttonLayout);

    // Connect signals
    connect(m_confirmButton, &QPushButton::clicked, this,
            &QtPasswordConfirmDialog::onConfirmClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QtPasswordConfirmDialog::onCancelClicked);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this,
            &QtPasswordConfirmDialog::onConfirmClicked);
}

void QtPasswordConfirmDialog::applyTheme() {
    // Dialog background
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, m_themeManager->backgroundColor());
    setPalette(palette);
    setAutoFillBackground(true);

    // Title styling
    QString titleStyle = QString(R"(
        QLabel {
            color: %1;
            background-color: transparent;
        }
    )")
                             .arg(m_themeManager->textColor().name());
    m_titleLabel->setStyleSheet(titleStyle);

    // Message styling
    QString messageStyle = QString(R"(
        QLabel {
            color: %1;
            background-color: transparent;
        }
    )")
                               .arg(m_themeManager->subtitleColor().name());
    m_messageLabel->setStyleSheet(messageStyle);

    // Password input styling
    QString inputStyle = QString(R"(
        QLineEdit {
            background-color: %1;
            color: %2;
            border: 2px solid %3;
            border-radius: 6px;
            padding: 8px 12px;
        }
        QLineEdit:focus {
            border-color: %4;
        }
    )")
                             .arg(m_themeManager->surfaceColor().name())
                             .arg(m_themeManager->textColor().name())
                             .arg(m_themeManager->secondaryColor().name())
                             .arg(m_themeManager->accentColor().name());
    m_passwordEdit->setStyleSheet(inputStyle);

    // Error label styling
    QString errorStyle = QString(R"(
        QLabel {
            color: #ff4444;
            background-color: transparent;
        }
    )");
    m_errorLabel->setStyleSheet(errorStyle);

    // Confirm button styling
    QString confirmStyle = QString(R"(
        QPushButton {
            background-color: %1;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px 20px;
            font-weight: 600;
        }
        QPushButton:hover {
            background-color: %2;
        }
        QPushButton:pressed {
            background-color: %3;
        }
    )")
                               .arg(m_themeManager->accentColor().name())
                               .arg(m_themeManager->accentColor().lighter(110).name())
                               .arg(m_themeManager->accentColor().darker(110).name());
    m_confirmButton->setStyleSheet(confirmStyle);

    // Cancel button styling
    QString cancelStyle = QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 10px 20px;
            font-weight: 500;
        }
        QPushButton:hover {
            background-color: %3;
            border-color: %3;
        }
        QPushButton:pressed {
            background-color: %4;
        }
    )")
                              .arg(m_themeManager->surfaceColor().name())
                              .arg(m_themeManager->textColor().name())
                              .arg(m_themeManager->secondaryColor().name())
                              .arg(m_themeManager->secondaryColor().darker(110).name());
    m_cancelButton->setStyleSheet(cancelStyle);
}

void QtPasswordConfirmDialog::showError(const QString& error) {
    m_errorLabel->setText(error);
    m_errorLabel->show();
}

void QtPasswordConfirmDialog::onConfirmClicked() {
    QString password = m_passwordEdit->text();

    if (password.isEmpty()) {
        showError("Password cannot be empty.");
        return;
    }

    // Verify password against database
    Auth::AuthResponse response = Auth::LoginUser(m_username.toStdString(), password.toStdString());

    if (response.success()) {
        // Password confirmed
        m_password = password;
        m_confirmed = true;
        accept();
    } else {
        // Password incorrect
        showError("Invalid password. Please try again.");
        m_passwordEdit->clear();
        m_passwordEdit->setFocus();
    }
}

void QtPasswordConfirmDialog::onCancelClicked() {
    m_confirmed = false;
    reject();
}
