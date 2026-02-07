#pragma once

#include <QDialog>
#include <QProcess>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QListWidget>
#include <QMap>

class QtTestConsole : public QDialog {
    Q_OBJECT

public:
    explicit QtTestConsole(QWidget *parent = nullptr);
    ~QtTestConsole();

    // Run a specific test suite
    void runTest(const QString &testName, const QString &executableName);
    
    // Run all available tests
    void runAllTests();

private slots:
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onRunClicked();
    void onCloseClicked();

private:
    void setupUI();
    void appendOutput(const QString &text);
    QString findTestExecutable(const QString &name);
    void processNextTest();

    QTextEdit *m_consoleOutput;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_runButton;
    QPushButton *m_closeButton;
    QListWidget *m_testListWidget;
    
    QProcess *m_currentProcess;
    QMap<QString, QString> m_availableTests;
    QList<QString> m_testQueue;
    bool m_isRunning;
    int m_totalTests;
    int m_passedTests;
};
