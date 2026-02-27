#pragma once

#include <QTextEdit>
#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

class QtTestConsole : public QDialog {
    Q_OBJECT

  public:
    explicit QtTestConsole(QWidget* parent = nullptr);
    ~QtTestConsole();

    // Run a specific test suite
    void runTest(const QString& testName, const QString& executableName);

    // Run all available tests
    void runAllTests();

    void done(int r) override;

  private slots:
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onRunClicked();
    void onRunSelectedClicked();
    void onTestDoubleClicked(QListWidgetItem* item);
    void onCloseClicked();

  private:
    void setupUI();
    void appendOutput(const QString& text);
    QString findTestExecutable(const QString& name);
    void processNextTest();
    void updateTestItemStatus(const QString& testName, bool passed);
    void resetTestListStyles();

    QTextEdit* m_consoleOutput;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QPushButton* m_runButton;
    QPushButton* m_runSelectedButton;
    QPushButton* m_closeButton;
    QListWidget* m_testListWidget;

    QProcess* m_currentProcess;
    QMap<QString, QString> m_availableTests;
    QMap<QString, QString> m_testResults;
    QList<QString> m_testQueue;
    bool m_isRunning;
    bool m_runningSingleTest;
    QString m_currentTestName;
    int m_totalTests;
    int m_passedTests;
};
