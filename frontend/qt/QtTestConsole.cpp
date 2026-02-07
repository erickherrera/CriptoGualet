#include "QtTestConsole.h"
#include "QtThemeManager.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QScrollBar>
#include <QDateTime>
#include <QRegularExpression>

QtTestConsole::QtTestConsole(QWidget *parent)
    : QDialog(parent), m_currentProcess(nullptr), m_isRunning(false), m_totalTests(0), m_passedTests(0) {
    setWindowTitle("System Diagnostics & Security Verification");
    resize(900, 600);
    
    // Define available tests based on the project structure
    // These map readable names to executable names
    m_availableTests["Secure Seed Storage (DPAPI)"] = "test_secure_seed";
    m_availableTests["Password Verification & Hashing"] = "test_password_verification";
    m_availableTests["BIP39/BIP44 Wallet Chains"] = "test_wallet_chains";
    m_availableTests["Session Management (Auth)"] = "test_session_consolidated";
    m_availableTests["Repository & Database"] = "test_repository_consolidated";
    m_availableTests["Security Enhancements"] = "test_security_enhancements";
    m_availableTests["2FA / TOTP"] = "test_2fa";
    m_availableTests["BlockCypher API"] = "test_blockcypher_api";

    setupUI();
    
    m_currentProcess = new QProcess(this);
    connect(m_currentProcess, &QProcess::readyReadStandardOutput, this, &QtTestConsole::onProcessReadyReadStandardOutput);
    connect(m_currentProcess, &QProcess::readyReadStandardError, this, &QtTestConsole::onProcessReadyReadStandardError);
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &QtTestConsole::onProcessFinished);
}

QtTestConsole::~QtTestConsole() {
    if (m_currentProcess && m_currentProcess->state() == QProcess::Running) {
        m_currentProcess->kill();
    }
}

void QtTestConsole::setupUI() {
    auto &theme = QtThemeManager::instance();
    
    // Apply basic styling
    setStyleSheet(QString(
        "QDialog { background-color: %1; color: %2; }"
        "QTextEdit { background-color: #1E1E1E; color: #D4D4D4; font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; border: 1px solid %3; border-radius: 4px; }"
        "QListWidget { background-color: %4; color: %2; border: 1px solid %3; border-radius: 4px; }"
        "QLabel { color: %2; font-weight: bold; }"
        "QPushButton { background-color: %5; color: white; border: none; padding: 8px 16px; border-radius: 4px; font-weight: bold; }"
        "QPushButton:hover { background-color: %6; }"
        "QPushButton:disabled { background-color: %7; color: #888; }"
        "QProgressBar { border: 1px solid %3; border-radius: 4px; text-align: center; }"
        "QProgressBar::chunk { background-color: %5; }"
    ).arg(theme.backgroundColor().name())
     .arg(theme.textColor().name())
     .arg(theme.secondaryColor().name())
     .arg(theme.surfaceColor().name())
     .arg(theme.accentColor().name())
     .arg(theme.accentColor().lighter(110).name())
     .arg(theme.secondaryColor().darker().name()));

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header
    QLabel *headerLabel = new QLabel("Run system diagnostics to verify the security and integrity of your installation.", this);
    headerLabel->setStyleSheet("font-size: 14px; margin-bottom: 10px;");
    mainLayout->addWidget(headerLabel);

    // Split view: Test List vs Console Output
    QHBoxLayout *contentLayout = new QHBoxLayout();
    
    // Left: Test List
    QVBoxLayout *listLayout = new QVBoxLayout();
    QLabel *listLabel = new QLabel("Available Diagnostic Tests:", this);
    m_testListWidget = new QListWidget(this);
    m_testListWidget->setFixedWidth(250);
    
    // Populate list
    for (auto it = m_availableTests.begin(); it != m_availableTests.end(); ++it) {
        m_testListWidget->addItem(it.key());
    }
    
    listLayout->addWidget(listLabel);
    listLayout->addWidget(m_testListWidget);
    contentLayout->addLayout(listLayout);

    // Right: Console Output
    QVBoxLayout *consoleLayout = new QVBoxLayout();
    QLabel *consoleLabel = new QLabel("Execution Log:", this);
    m_consoleOutput = new QTextEdit(this);
    m_consoleOutput->setReadOnly(true);
    
    consoleLayout->addWidget(consoleLabel);
    consoleLayout->addWidget(m_consoleOutput);
    contentLayout->addLayout(consoleLayout);

    mainLayout->addLayout(contentLayout);

    // Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);

    // Status Label
    m_statusLabel = new QLabel("Ready", this);
    mainLayout->addWidget(m_statusLabel);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_runButton = new QPushButton("Run All Diagnostics", this);
    connect(m_runButton, &QPushButton::clicked, this, &QtTestConsole::onRunClicked);
    
    m_closeButton = new QPushButton("Close", this);
    connect(m_closeButton, &QPushButton::clicked, this, &QtTestConsole::onCloseClicked);
    
    buttonLayout->addWidget(m_runButton);
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);
}

void QtTestConsole::appendOutput(const QString &text) {
    // Basic ANSI color code parsing for the console
    QString html = text.toHtmlEscaped();
    
    // Convert basic ANSI colors to HTML spans
    // Green
    html.replace(QRegularExpression("\x1b\[32m"), "<span style='color:#4CAF50;'>");
    // Red
    html.replace(QRegularExpression("\x1b\[31m"), "<span style='color:#F44336;'>");
    // Blue
    html.replace(QRegularExpression("\x1b\[34m"), "<span style='color:#2196F3;'>");
    // Cyan
    html.replace(QRegularExpression("\x1b\[36m"), "<span style='color:#00BCD4;'>");
    // Reset
    html.replace(QRegularExpression("\x1b\[0m"), "</span>");
    
    // Replace newlines
    html.replace("
", "<br>");

    m_consoleOutput->moveCursor(QTextCursor::End);
    m_consoleOutput->insertHtml(html);
    m_consoleOutput->moveCursor(QTextCursor::End);
    m_consoleOutput->verticalScrollBar()->setValue(m_consoleOutput->verticalScrollBar()->maximum());
}

QString QtTestConsole::findTestExecutable(const QString &name) {
    // Look in the application directory
    QString appDir = QCoreApplication::applicationDirPath();
    QString exeName = name;
#ifdef Q_OS_WIN
    if (!exeName.endsWith(".exe")) {
        exeName += ".exe";
    }
#endif
    
    QFileInfo check(QDir(appDir).filePath(exeName));
    if (check.exists() && check.isExecutable()) {
        return check.absoluteFilePath();
    }
    
    // Fallback: try looking in parent directories (for development environments)
    // This helps when running from IDEs where CWD might vary
    QDir dir(appDir);
    for (int i = 0; i < 3; ++i) {
        dir.cdUp();
        check.setFile(dir.filePath(exeName));
        if (check.exists() && check.isExecutable()) {
            return check.absoluteFilePath();
        }
        // Try bin subdirectory
        check.setFile(dir.filePath("bin/" + exeName));
        if (check.exists() && check.isExecutable()) {
            return check.absoluteFilePath();
        }
    }

    return QString();
}

void QtTestConsole::runTest(const QString &testName, const QString &executableName) {
    QString exePath = findTestExecutable(executableName);
    
    appendOutput(QString("<b>Running: %1</b><br>").arg(testName));
    
    if (exePath.isEmpty()) {
        appendOutput(QString("<span style='color:#F44336;'>Error: Could not find executable '%1'.</span><br>").arg(executableName));
        appendOutput("Please ensure the test suite is installed.<br>");
        appendOutput("--------------------------------------------------<br>");
        processNextTest();
        return;
    }

    m_currentProcess->start(exePath, QStringList());
    if (!m_currentProcess->waitForStarted()) {
        appendOutput(QString("<span style='color:#F44336;'>Error: Failed to start process '%1'.</span><br>").arg(exePath));
        processNextTest();
    }
}

void QtTestConsole::runAllTests() {
    if (m_isRunning) return;
    
    m_isRunning = true;
    m_runButton->setEnabled(false);
    m_consoleOutput->clear();
    m_progressBar->setValue(0);
    m_testQueue = m_availableTests.keys();
    m_totalTests = m_testQueue.size();
    m_passedTests = 0;
    
    appendOutput(QString("<b>Starting System Diagnostics - %1</b><br>").arg(QDateTime::currentDateTime().toString()));
    appendOutput("==================================================<br>");
    
    processNextTest();
}

void QtTestConsole::processNextTest() {
    if (m_testQueue.isEmpty()) {
        m_isRunning = false;
        m_runButton->setEnabled(true);
        m_statusLabel->setText(QString("Diagnostics Complete. Passed: %1/%2").arg(m_passedTests).arg(m_totalTests));
        m_progressBar->setValue(100);
        appendOutput("<br><b>All diagnostics completed.</b><br>");
        return;
    }

    int progress = ((m_totalTests - m_testQueue.size()) * 100) / m_totalTests;
    m_progressBar->setValue(progress);

    QString testName = m_testQueue.takeFirst();
    QString exeName = m_availableTests[testName];
    
    m_statusLabel->setText(QString("Running: %1...").arg(testName));
    runTest(testName, exeName);
}

void QtTestConsole::onProcessReadyReadStandardOutput() {
    QByteArray data = m_currentProcess->readAllStandardOutput();
    appendOutput(QString::fromLocal8Bit(data));
}

void QtTestConsole::onProcessReadyReadStandardError() {
    QByteArray data = m_currentProcess->readAllStandardError();
    appendOutput(QString("<span style='color:#FF9800;'>%1</span>").arg(QString::fromLocal8Bit(data).toHtmlEscaped()));
}

void QtTestConsole::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        appendOutput("<span style='color:#4CAF50;'><b>✓ TEST PASSED</b></span><br>");
        m_passedTests++;
    } else {
        appendOutput(QString("<span style='color:#F44336;'><b>✗ TEST FAILED (Exit Code: %1)</b></span><br>").arg(exitCode));
    }
    appendOutput("--------------------------------------------------<br>");
    
    // Schedule next test
    processNextTest();
}

void QtTestConsole::onRunClicked() {
    runAllTests();
}

void QtTestConsole::onCloseClicked() {
    if (m_isRunning) {
        m_currentProcess->kill();
    }
    accept();
}
