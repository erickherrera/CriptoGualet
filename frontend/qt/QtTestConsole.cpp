#include "QtTestConsole.h"
#include "QtThemeManager.h"
#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QScrollBar>

QtTestConsole::QtTestConsole(QWidget* parent)
    : QDialog(parent),
      m_currentProcess(nullptr),
      m_isRunning(false),
      m_runningSingleTest(false),
      m_totalTests(0),
      m_passedTests(0) {
    setWindowTitle("System Diagnostics & Security Verification");
    resize(900, 600);

    // Define available tests based on the project structure
    // These map readable names to executable names
    m_availableTests["Secure Seed Storage (DPAPI)"] = "test_secure_seed";
    // m_availableTests["Password Verification & Hashing"] = "test_password_verification";
    m_availableTests["BIP39/BIP44 Wallet Chains"] = "test_wallet_chains";
    // m_availableTests["Session Management (Auth)"] = "test_session_consolidated";
    // m_availableTests["Security Enhancements"] = "test_security_enhancements";
    // m_availableTests["2FA / TOTP"] = "test_2fa";
    m_availableTests["BlockCypher API"] = "test_blockcypher_api";

    setupUI();

    // Populate test list after UI is set up
    for (auto it = m_availableTests.begin(); it != m_availableTests.end(); ++it) {
        m_testListWidget->addItem(it.key());
        m_testResults[it.key()] = "";  // Not run yet
    }

    m_currentProcess = new QProcess(this);
    connect(m_currentProcess, &QProcess::readyReadStandardOutput, this,
            &QtTestConsole::onProcessReadyReadStandardOutput);
    connect(m_currentProcess, &QProcess::readyReadStandardError, this,
            &QtTestConsole::onProcessReadyReadStandardError);
    connect(m_currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &QtTestConsole::onProcessFinished);

    // Connect double-click signal for running individual tests
    connect(m_testListWidget, &QListWidget::itemDoubleClicked, this,
            &QtTestConsole::onTestDoubleClicked);
}

QtTestConsole::~QtTestConsole() {
    // Stop the queue and disconnect signals to prevent callbacks during destruction
    m_testQueue.clear();
    if (m_currentProcess) {
        m_currentProcess->disconnect(this);
        if (m_currentProcess->state() != QProcess::NotRunning) {
            m_currentProcess->kill();
            m_currentProcess->waitForFinished(500);
        }
    }
}

void QtTestConsole::setupUI() {
    auto& theme = QtThemeManager::instance();

    // Apply basic styling
    setStyleSheet(
        QString("QDialog { background-color: %1; color: %2; }"
                "QTextEdit { background-color: #0D1117; color: #C9D1D9; font-family: 'Consolas', "
                "'Courier New', monospace; font-size: 12px; border: 1px solid #30363D; "
                "border-radius: 6px; padding: 8px; }"
                "QListWidget { background-color: %4; color: %2; border: 1px solid %3; "
                "border-radius: 4px; }"
                "QListWidget::item { padding: 8px; border-radius: 4px; margin: 2px; }"
                "QListWidget::item:selected { background-color: %5; color: white; }"
                "QLabel { color: %2; font-weight: bold; }"
                "QPushButton { background-color: %5; color: white; border: none; padding: 10px "
                "20px; border-radius: 6px; font-weight: bold; font-size: 13px; }"
                "QPushButton:hover { background-color: %6; }"
                "QPushButton:disabled { background-color: #21262D; color: #484F58; }"
                "QProgressBar { border: 1px solid #30363D; border-radius: 4px; text-align: center; "
                "background-color: #161B22; }"
                "QProgressBar::chunk { background-color: %5; border-radius: 3px; }")
            .arg(theme.backgroundColor().name())
            .arg(theme.textColor().name())
            .arg(theme.secondaryColor().name())
            .arg(theme.surfaceColor().name())
            .arg(theme.accentColor().name())
            .arg(theme.accentColor().lighter(110).name())
            .arg(theme.secondaryColor().darker().name()));

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header with icon
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* iconLabel = new QLabel(this);
    iconLabel->setText("<span style='font-size: 24px;'>🛡️</span>");
    headerLayout->addWidget(iconLabel);

    QVBoxLayout* titleLayout = new QVBoxLayout();
    QLabel* titleLabel = new QLabel("System Diagnostics & Security Verification", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #58A6FF;");
    QLabel* headerLabel = new QLabel(
        "Run diagnostics to verify the security and integrity of your installation.", this);
    headerLabel->setStyleSheet("font-size: 12px; color: #8B949E; margin-bottom: 5px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(headerLabel);
    headerLayout->addLayout(titleLayout);
    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    // Split view: Test List vs Console Output
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(15);

    // Left: Test List with status indicators
    QVBoxLayout* listLayout = new QVBoxLayout();
    QLabel* listLabel = new QLabel("Available Tests:", this);
    listLabel->setStyleSheet("color: #58A6FF; font-size: 13px; margin-bottom: 5px;");
    m_testListWidget = new QListWidget(this);
    m_testListWidget->setFixedWidth(280);
    m_testListWidget->setSpacing(4);

    listLayout->addWidget(listLabel);
    listLayout->addWidget(m_testListWidget);
    contentLayout->addLayout(listLayout);

    // Right: Console Output
    QVBoxLayout* consoleLayout = new QVBoxLayout();
    QLabel* consoleLabel = new QLabel("Execution Log:", this);
    consoleLabel->setStyleSheet("color: #58A6FF; font-size: 13px; margin-bottom: 5px;");
    m_consoleOutput = new QTextEdit(this);
    m_consoleOutput->setReadOnly(true);
    m_consoleOutput->setStyleSheet("QTextEdit { selection-background-color: #264F78; }");

    consoleLayout->addWidget(consoleLabel);
    consoleLayout->addWidget(m_consoleOutput);
    contentLayout->addLayout(consoleLayout);

    mainLayout->addLayout(contentLayout);

    // Progress Bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(8);
    mainLayout->addWidget(m_progressBar);

    // Status Label with icons
    m_statusLabel = new QLabel("Ready to run diagnostics", this);
    m_statusLabel->setStyleSheet("color: #8B949E; font-size: 12px;");
    mainLayout->addWidget(m_statusLabel);

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_runSelectedButton = new QPushButton("▶ Run Selected", this);
    connect(m_runSelectedButton, &QPushButton::clicked, this, &QtTestConsole::onRunSelectedClicked);

    m_runButton = new QPushButton("▶ Run All Diagnostics", this);
    connect(m_runButton, &QPushButton::clicked, this, &QtTestConsole::onRunClicked);

    m_closeButton = new QPushButton("Close", this);
    connect(m_closeButton, &QPushButton::clicked, this, &QtTestConsole::onCloseClicked);

    buttonLayout->addWidget(m_runSelectedButton);
    buttonLayout->addWidget(m_runButton);
    buttonLayout->addWidget(m_closeButton);
    mainLayout->addLayout(buttonLayout);
}

void QtTestConsole::appendOutput(const QString& text) {
    // Basic ANSI color code parsing for the console
    QString html = text.toHtmlEscaped();

    // Convert basic ANSI colors to HTML spans
    // We match the ESC character (\x1b) followed by [ and the color code
    html.replace(QRegularExpression("\x1b\\[32m"), QStringLiteral("<span style='color:#4CAF50;'>"));
    html.replace(QRegularExpression("\x1b\\[31m"), QStringLiteral("<span style='color:#F44336;'>"));
    html.replace(QRegularExpression("\x1b\\[34m"), QStringLiteral("<span style='color:#2196F3;'>"));
    html.replace(QRegularExpression("\x1b\\[36m"), QStringLiteral("<span style='color:#00BCD4;'>"));
    html.replace(QRegularExpression("\x1b\\[0m"), QStringLiteral("</span>"));

    // Replace newlines with HTML line breaks
    html.replace(QLatin1Char('\n'), QStringLiteral("<br>"));

    m_consoleOutput->moveCursor(QTextCursor::End);
    m_consoleOutput->insertHtml(html);
    m_consoleOutput->moveCursor(QTextCursor::End);
    m_consoleOutput->verticalScrollBar()->setValue(m_consoleOutput->verticalScrollBar()->maximum());
}

QString QtTestConsole::findTestExecutable(const QString& name) {
    QString appDir = QCoreApplication::applicationDirPath();
    QString exeName = name;
#ifdef Q_OS_WIN
    if (!exeName.endsWith(".exe")) {
        exeName += ".exe";
    }
#endif

    appendOutput(QString("<b>Diagnostic:</b> App Dir: %1").arg(appDir));

    // Direct check in appDir
    QDir dir(appDir);
    if (dir.exists(exeName)) {
        appendOutput(QString("<b>Diagnostic:</b> Found %1 in App Dir").arg(exeName));
        return dir.absoluteFilePath(exeName);
    } else {
        appendOutput(QString("<b>Diagnostic:</b> %1 NOT found in App Dir").arg(exeName));
    }

    QStringList searchPaths;
    searchPaths << appDir;
    searchPaths << QDir(appDir).filePath("Release");
    searchPaths << QDir(appDir).filePath("Debug");
    searchPaths << QDir(appDir).filePath("bin");
    searchPaths << QDir(appDir).filePath("bin/Release");
    searchPaths << QDir(appDir).filePath("bin/Debug");

    for (const QString& path : searchPaths) {
        QString fullPath = QDir(path).filePath(exeName);
        QFileInfo check(fullPath);
        if (check.exists()) {
            return check.absoluteFilePath();
        }
    }

    QDir dirUp(appDir);
    for (int i = 0; i < 3; ++i) {
        dirUp.cdUp();
        QStringList parentSearchPaths;
        parentSearchPaths << dirUp.filePath(exeName);
        parentSearchPaths << dirUp.filePath("bin/" + exeName);
        parentSearchPaths << dirUp.filePath("bin/Release/" + exeName);
        parentSearchPaths << dirUp.filePath("bin/Debug/" + exeName);

        for (const QString& path : parentSearchPaths) {
            QFileInfo check(path);
            if (check.exists()) {
                return check.absoluteFilePath();
            }
        }
    }

    return QString();
}

void QtTestConsole::runTest(const QString& testName, const QString& executableName) {
    m_currentTestName = testName;
    QString exePath = findTestExecutable(executableName);

    appendOutput(QString("<b>Running: %1</b><br>").arg(testName));

    if (exePath.isEmpty()) {
        appendOutput(
            QString(
                "<span style='color:#F44336;'>Error: Could not find executable '%1'.</span><br>")
                .arg(executableName));
        appendOutput("Please ensure the test suite is installed.<br>");
        appendOutput("--------------------------------------------------<br>");
        m_testResults[m_currentTestName] = "failed";
        updateTestItemStatus(m_currentTestName, false);

        if (m_runningSingleTest) {
            m_isRunning = false;
            m_runButton->setEnabled(true);
            m_runSelectedButton->setEnabled(true);
            m_statusLabel->setText("Test Failed - Executable not found");
            m_progressBar->setValue(100);
        } else {
            processNextTest();
        }
        return;
    }

    m_currentProcess->start(exePath, QStringList());
    if (!m_currentProcess->waitForStarted()) {
        appendOutput(
            QString("<span style='color:#F44336;'>Error: Failed to start process '%1'.</span><br>")
                .arg(exePath));
        appendOutput("--------------------------------------------------<br>");
        m_testResults[m_currentTestName] = "failed";
        updateTestItemStatus(m_currentTestName, false);

        if (m_runningSingleTest) {
            m_isRunning = false;
            m_runButton->setEnabled(true);
            m_runSelectedButton->setEnabled(true);
            m_statusLabel->setText("Test Failed - Could not start process");
            m_progressBar->setValue(100);
        } else {
            processNextTest();
        }
    }
}

void QtTestConsole::runAllTests() {
    if (m_isRunning)
        return;

    m_isRunning = true;
    m_runningSingleTest = false;
    m_runButton->setEnabled(false);
    m_runSelectedButton->setEnabled(false);
    m_consoleOutput->clear();
    m_progressBar->setValue(0);
    m_testQueue = m_availableTests.keys();
    m_totalTests = (int)m_testQueue.size();
    m_passedTests = 0;

    // Reset test results
    for (auto it = m_testResults.begin(); it != m_testResults.end(); ++it) {
        it.value() = "";
    }
    resetTestListStyles();

    appendOutput(QString("<b>Starting System Diagnostics - %1</b><br>")
                     .arg(QDateTime::currentDateTime().toString()));
    appendOutput("==================================================<br>");

    processNextTest();
}

void QtTestConsole::processNextTest() {
    if (m_testQueue.isEmpty()) {
        m_isRunning = false;
        m_runButton->setEnabled(true);
        m_runSelectedButton->setEnabled(true);
        m_statusLabel->setText(
            QString("Diagnostics Complete. Passed: %1/%2").arg(m_passedTests).arg(m_totalTests));
        m_progressBar->setValue(100);
        appendOutput("<br><b>All diagnostics completed.</b><br>");
        return;
    }

    int progress = ((m_totalTests - (int)m_testQueue.size()) * 100) / m_totalTests;
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
    appendOutput(QString("<span style='color:#FF9800;'>%1</span>")
                     .arg(QString::fromLocal8Bit(data).toHtmlEscaped()));
}

void QtTestConsole::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    bool passed = (exitStatus == QProcess::NormalExit && exitCode == 0);

    if (passed) {
        appendOutput("<span style='color:#4CAF50;'><b>✓ TEST PASSED</b></span><br>");
        m_passedTests++;
        m_testResults[m_currentTestName] = "passed";
    } else {
        appendOutput(
            QString("<span style='color:#F44336;'><b>✗ TEST FAILED (Exit Code: %1)</b></span><br>")
                .arg(exitCode));
        m_testResults[m_currentTestName] = "failed";
    }
    appendOutput("--------------------------------------------------<br>");

    // Update visual status in the list
    updateTestItemStatus(m_currentTestName, passed);

    if (m_runningSingleTest) {
        // Single test completed
        m_isRunning = false;
        m_runButton->setEnabled(true);
        m_runSelectedButton->setEnabled(true);
        m_statusLabel->setText(passed ? "Test Passed" : "Test Failed");
        m_progressBar->setValue(100);
    } else {
        // Schedule next test in batch
        processNextTest();
    }
}

void QtTestConsole::onRunClicked() {
    runAllTests();
}

void QtTestConsole::onRunSelectedClicked() {
    if (m_isRunning)
        return;

    QListWidgetItem* selectedItem = m_testListWidget->currentItem();
    if (!selectedItem) {
        m_statusLabel->setText("Please select a test to run");
        appendOutput(
            "<span style='color:#FF9800;'>Please select a test from the list to run.</span><br>");
        return;
    }

    QString testName = selectedItem->text();
    // Strip status prefix if present
    if (testName.startsWith("✓ ") || testName.startsWith("✗ ")) {
        testName = testName.mid(2);
    }

    if (!m_availableTests.contains(testName)) {
        m_statusLabel->setText("Invalid test selected");
        return;
    }

    // Run single test
    m_isRunning = true;
    m_runningSingleTest = true;
    m_runButton->setEnabled(false);
    m_runSelectedButton->setEnabled(false);
    m_consoleOutput->clear();
    m_progressBar->setValue(0);
    m_totalTests = 1;
    m_passedTests = 0;

    appendOutput(QString("<b>Running: %1</b><br>").arg(testName));
    appendOutput("==================================================<br>");

    QString exeName = m_availableTests[testName];
    runTest(testName, exeName);
}

void QtTestConsole::onTestDoubleClicked(QListWidgetItem* item) {
    if (m_isRunning)
        return;
    if (!item)
        return;

    QString testName = item->text();
    // Strip status prefix if present
    if (testName.startsWith("✓ ") || testName.startsWith("✗ ")) {
        testName = testName.mid(2);
    }

    if (!m_availableTests.contains(testName))
        return;

    // Run single test
    m_isRunning = true;
    m_runningSingleTest = true;
    m_runButton->setEnabled(false);
    m_runSelectedButton->setEnabled(false);
    m_consoleOutput->clear();
    m_progressBar->setValue(0);
    m_totalTests = 1;
    m_passedTests = 0;

    appendOutput(QString("<b>Running: %1</b><br>").arg(testName));
    appendOutput("==================================================<br>");

    QString exeName = m_availableTests[testName];
    runTest(testName, exeName);
}

void QtTestConsole::onCloseClicked() {
    done(QDialog::Rejected);
}

void QtTestConsole::done(int r) {
    m_isRunning = false;
    m_testQueue.clear();

    if (m_currentProcess) {
        // Disconnect all signals so we don't get any more callbacks
        m_currentProcess->disconnect(this);

        if (m_currentProcess->state() != QProcess::NotRunning) {
            m_currentProcess->kill();
            m_currentProcess->waitForFinished(500);
        }
    }

    QDialog::done(r);
}

void QtTestConsole::updateTestItemStatus(const QString& testName, bool passed) {
    // Search for item by comparing after stripping any existing prefix
    QListWidgetItem* targetItem = nullptr;
    for (int i = 0; i < m_testListWidget->count(); ++i) {
        QListWidgetItem* item = m_testListWidget->item(i);
        QString itemText = item->text();
        // Strip status prefix if present
        if (itemText.startsWith("✓ ") || itemText.startsWith("✗ ")) {
            itemText = itemText.mid(2);
        }
        if (itemText == testName) {
            targetItem = item;
            break;
        }
    }

    if (!targetItem)
        return;

    // Add new status prefix
    QString text;
    if (passed) {
        text = "✓ " + testName;
        targetItem->setBackground(QColor("#1A4D1A"));  // Dark green
        targetItem->setForeground(QColor("#4CAF50"));  // Bright green
    } else {
        text = "✗ " + testName;
        targetItem->setBackground(QColor("#4D1A1A"));  // Dark red
        targetItem->setForeground(QColor("#F44336"));  // Bright red
    }

    targetItem->setText(text);
}

void QtTestConsole::resetTestListStyles() {
    for (int i = 0; i < m_testListWidget->count(); ++i) {
        QListWidgetItem* item = m_testListWidget->item(i);
        QString text = item->text();

        // Remove status prefix
        if (text.startsWith("✓ ")) {
            text = text.mid(2);
        } else if (text.startsWith("✗ ")) {
            text = text.mid(2);
        }

        item->setText(text);
        item->setBackground(QBrush());  // Reset to default
        item->setForeground(QBrush());  // Reset to default
    }
}
