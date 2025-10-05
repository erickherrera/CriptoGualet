#pragma once

#include <QDialog>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>
#include <QGroupBox>
#include <QGridLayout>
#include <QPixmap>
#include <string>
#include <vector>
#include "QRGenerator.h"

class QtSeedDisplayDialog : public QDialog {
    Q_OBJECT

public:
    explicit QtSeedDisplayDialog(const std::vector<std::string>& seedWords, QWidget* parent = nullptr);
    ~QtSeedDisplayDialog() override = default;

    bool userConfirmedBackup() const { return m_userConfirmed; }

private slots:
    void onConfirmBackup();
    void onShowQRCode();
    void onCopyToClipboard();

private:
    void setupUI();
    void setupSeedDisplay();
    void setupQRDisplay();
    void generateQRCode();
    void createWordGrid();

    // QR code generation helper methods
    QImage createQRImage(const QR::QRData& qrData);
    QImage scaleAndPadQRImage(const QImage& qrImage, int originalWidth);
    void displayQRError(const QString& message);
    void displayQRWarning();

    std::vector<std::string> m_seedWords;
    bool m_userConfirmed = false;

    QVBoxLayout* m_mainLayout = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_scrollContent = nullptr;
    QVBoxLayout* m_scrollLayout = nullptr;
    QTextEdit* m_seedTextEdit = nullptr;
    QGridLayout* m_wordGrid = nullptr;
    QGroupBox* m_wordGroup = nullptr;
    QLabel* m_qrLabel = nullptr;
    QPushButton* m_showQRButton = nullptr;
    QPushButton* m_copyButton = nullptr;
    QPushButton* m_confirmButton = nullptr;
    QCheckBox* m_confirmCheckbox = nullptr;

    QPixmap m_qrPixmap;
    bool m_qrGenerated = false;
};