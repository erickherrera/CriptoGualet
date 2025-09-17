#include "../include/QtSeedDisplayDialog.h"
#include "../include/QRGenerator.h"
#include "../include/QtThemeManager.h"
#include <QApplication>
#include <QClipboard>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QPalette>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <iomanip>
#include <sstream>

QtSeedDisplayDialog::QtSeedDisplayDialog(
    const std::vector<std::string> &seedWords, QWidget *parent)
    : QDialog(parent), m_seedWords(seedWords) {
  setWindowTitle("Backup Your Seed Phrase");
  setModal(true);
  setMinimumSize(500, 600); // Optimized for laptop screens - taller than wide
  resize(500, 700);         // Default size for laptops

  // Apply theme styling to the dialog
  QtThemeManager &theme = QtThemeManager::instance();
  setStyleSheet(QString("QDialog { background-color: %1; color: %2; }")
                    .arg(theme.backgroundColor().name())
                    .arg(theme.textColor().name()));

  setupUI();
  setupSeedDisplay();
  generateQRCode(); // Generate QR code immediately
}

void QtSeedDisplayDialog::setupUI() {
  m_mainLayout = new QVBoxLayout(this);
  m_mainLayout->setSpacing(5);
  m_mainLayout->setContentsMargins(10, 10, 10, 10);

  // Title - compact
  QtThemeManager &theme = QtThemeManager::instance();
  QLabel *titleLabel = new QLabel("🔐 BACKUP YOUR SEED PHRASE");
  titleLabel->setAlignment(Qt::AlignCenter);
  QFont compactTitleFont = theme.titleFont();
  compactTitleFont.setPointSize(12); // Smaller font size for compact height
  titleLabel->setFont(compactTitleFont);
  titleLabel->setStyleSheet(QString("color: %1; padding: 2px; margin: 0px;")
                                .arg(theme.errorColor().name()));
  titleLabel->setMaximumHeight(30); // Limit height to make it more compact
  m_mainLayout->addWidget(titleLabel);

  // QR Code Section - Main Component
  QGroupBox *qrGroup = new QGroupBox("QR Code - Scan to backup on mobile");
  qrGroup->setStyleSheet(QString("QGroupBox { font-weight: bold; color: %1; }")
                             .arg(theme.accentColor().name()));
  QVBoxLayout *qrLayout = new QVBoxLayout(qrGroup);
  qrLayout->setContentsMargins(8, 8, 8, 5); // Reduced margins for compactness

  m_qrLabel = new QLabel();
  m_qrLabel->setAlignment(Qt::AlignCenter);
  m_qrLabel->setMinimumSize(160, 160); // Larger for better scanning
  m_qrLabel->setMaximumSize(180, 180); // Larger for better scanning
  m_qrLabel->setStyleSheet(QString("border: 2px solid %1; background-color: "
                                   "white; border-radius: 6px; padding: 6px;")
                               .arg(theme.accentColor().name()));
  m_qrLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  // Center the QR code in the layout
  qrLayout->addWidget(m_qrLabel, 0, Qt::AlignHCenter);

  // Set maximum height for the entire QR group
  qrGroup->setMaximumHeight(220); // Increased for larger QR code

  m_mainLayout->addWidget(qrGroup);

  // Create scroll area for the word list
  m_scrollArea = new QScrollArea();
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_scrollArea->setMaximumHeight(200); // Limit height to force scrolling
  m_scrollArea->setStyleSheet("QScrollArea { border: none; }");

  m_scrollContent = new QWidget();
  m_scrollLayout = new QVBoxLayout(m_scrollContent);
  m_scrollLayout->setContentsMargins(5, 5, 5, 5);

  // Warning - compact
  QLabel *warningLabel =
      new QLabel("⚠️ Write these words on paper and store safely!");
  warningLabel->setWordWrap(true);
  warningLabel->setAlignment(Qt::AlignCenter);
  warningLabel->setStyleSheet(
      QString("background-color: %1; color: %2; padding: 8px; border: 1px "
              "solid %3; border-radius: 4px; font-size: 11px;")
          .arg(theme.warningColor().lighter(180).name())
          .arg(theme.warningColor().darker(200).name())
          .arg(theme.warningColor().name()));
  m_scrollLayout->addWidget(warningLabel);

  // Word Grid Section
  m_wordGroup = new QGroupBox("12-Word Seed Phrase (Copy-Paste Ready):");
  m_wordGroup->setStyleSheet(
      QString("QGroupBox { font-weight: bold; color: %1; }")
          .arg(theme.textColor().name()));
  QVBoxLayout *wordLayout = new QVBoxLayout(m_wordGroup);

  createWordGrid();
  wordLayout->addLayout(m_wordGrid);

  // Full text version for easy copying
  m_seedTextEdit = new QTextEdit();
  m_seedTextEdit->setReadOnly(true);
  m_seedTextEdit->setMaximumHeight(60); // Compact
  m_seedTextEdit->setFont(theme.monoFont());
  m_seedTextEdit->setStyleSheet(
      QString("background-color: %1; color: %2; border: 1px solid %3; padding: "
              "5px; border-radius: 4px;")
          .arg(theme.surfaceColor().darker(120).name())
          .arg(theme.textColor().name())
          .arg(theme.secondaryColor().name()));
  wordLayout->addWidget(m_seedTextEdit);

  // Copy button for easy clipboard access
  m_copyButton = new QPushButton("Copy All Words");
  m_copyButton->setFont(theme.buttonFont());
  m_copyButton->setStyleSheet(
      QString("QPushButton { background-color: %1; color: white; padding: 8px "
              "12px; border: none; border-radius: 4px; font-weight: bold; } "
              "QPushButton:hover { background-color: %2; }")
          .arg(theme.accentColor().name())
          .arg(theme.accentColor().darker(120).name()));
  connect(m_copyButton, &QPushButton::clicked, this,
          &QtSeedDisplayDialog::onCopyToClipboard);
  wordLayout->addWidget(m_copyButton);

  m_scrollLayout->addWidget(m_wordGroup);

  m_scrollArea->setWidget(m_scrollContent);
  m_mainLayout->addWidget(m_scrollArea);

  // Confirmation section - always visible at bottom
  QFrame *line = new QFrame();
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  m_mainLayout->addWidget(line);

  m_confirmCheckbox = new QCheckBox("I have safely stored my seed phrase");
  m_confirmCheckbox->setFont(theme.textFont());
  m_confirmCheckbox->setStyleSheet(
      QString("QCheckBox { font-weight: bold; padding: 5px; color: %1; }")
          .arg(theme.textColor().name()));
  m_mainLayout->addWidget(m_confirmCheckbox);

  QHBoxLayout *buttonLayout = new QHBoxLayout();

  m_confirmButton = new QPushButton("Continue");
  m_confirmButton->setEnabled(false);
  m_confirmButton->setFont(theme.buttonFont());
  m_confirmButton->setStyleSheet(
      QString("QPushButton { background-color: %1; color: white; padding: 10px "
              "16px; border: none; border-radius: 5px; font-weight: bold; } "
              "QPushButton:hover:enabled { background-color: %2; } "
              "QPushButton:disabled { background-color: %3; }")
          .arg(theme.accentColor().name())
          .arg(theme.accentColor().darker(120).name())
          .arg(theme.secondaryColor().name()));
  connect(m_confirmButton, &QPushButton::clicked, this,
          &QtSeedDisplayDialog::onConfirmBackup);
  connect(m_confirmCheckbox, &QCheckBox::toggled, m_confirmButton,
          &QPushButton::setEnabled);

  QPushButton *cancelButton = new QPushButton("Cancel");
  cancelButton->setFont(theme.buttonFont());
  cancelButton->setStyleSheet(
      QString("QPushButton { background-color: %1; color: white; padding: 10px "
              "16px; border: none; border-radius: 5px; } QPushButton:hover { "
              "background-color: %2; }")
          .arg(theme.secondaryColor().darker(110).name())
          .arg(theme.secondaryColor().darker(130).name()));
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

  buttonLayout->addStretch();
  buttonLayout->addWidget(cancelButton);
  buttonLayout->addWidget(m_confirmButton);

  m_mainLayout->addLayout(buttonLayout);
}

void QtSeedDisplayDialog::createWordGrid() {
  m_wordGrid = new QGridLayout();
  m_wordGrid->setSpacing(8);

  // Create a nice grid layout with numbered words
  int cols = 3; // 3 columns for compact layout
  for (size_t i = 0; i < m_seedWords.size(); ++i) {
    int row = static_cast<int>(i) / cols;
    int col = static_cast<int>(i) % cols;

    QLabel *wordLabel =
        new QLabel(QString("%1. %2")
                       .arg(i + 1, 2, 10, QChar('0')) // Zero-padded number
                       .arg(QString::fromStdString(m_seedWords[i])));

    QtThemeManager &theme = QtThemeManager::instance();
    wordLabel->setFont(theme.monoFont());
    wordLabel->setStyleSheet(
        QString("QLabel { background-color: %1; color: %2; border: 1px solid "
                "%3; border-radius: 4px; padding: 8px; font-weight: bold; }")
            .arg(theme.surfaceColor().darker(120).name())
            .arg(theme.textColor().name())
            .arg(theme.secondaryColor().name()));
    wordLabel->setAlignment(Qt::AlignCenter);
    wordLabel->setMinimumHeight(30);

    m_wordGrid->addWidget(wordLabel, row, col);
  }
}

void QtSeedDisplayDialog::setupSeedDisplay() {
  // Create single-line copy-paste friendly format
  std::ostringstream oss;
  for (size_t i = 0; i < m_seedWords.size(); ++i) {
    if (i > 0)
      oss << " ";
    oss << m_seedWords[i];
  }

  m_seedTextEdit->setPlainText(QString::fromStdString(oss.str()));
}

void QtSeedDisplayDialog::onShowQRCode() {
  // This method is no longer needed since QR code is shown immediately
  // But keeping it for compatibility if referenced elsewhere
}

void QtSeedDisplayDialog::generateQRCode() {
  // Build seed phrase string
  std::ostringstream seedText;
  for (size_t i = 0; i < m_seedWords.size(); ++i) {
    if (i > 0)
      seedText << " ";
    seedText << m_seedWords[i];
  }

  // Generate QR code data
  QR::QRData qrData;
  bool qrSuccess = QR::GenerateQRCode(seedText.str(), qrData);

  // Check if we have valid QR data
  if (qrData.width <= 0 || qrData.height <= 0 || qrData.data.empty()) {
    displayQRError("QR Code Generation Failed\n\nPlease copy the words\nbelow manually");
    return;
  }

  // Create QR image from raw data
  QImage qrImage = createQRImage(qrData);
  if (qrImage.isNull()) {
    displayQRError("QR Image Creation Failed\n\nPlease copy the words\nbelow manually");
    return;
  }

  // Scale and add padding for optimal scanning
  QImage finalImage = scaleAndPadQRImage(qrImage, qrData.width);

  // Convert to pixmap and display
  m_qrPixmap = QPixmap::fromImage(finalImage);
  m_qrGenerated = true;
  m_qrLabel->setPixmap(m_qrPixmap);

  // Handle fallback pattern warning
  if (!qrSuccess) {
    displayQRWarning();
  }
}

QImage QtSeedDisplayDialog::createQRImage(const QR::QRData &qrData) {
  QImage qrImage(qrData.width, qrData.height, QImage::Format_RGB888);

  // Convert raw QR data to high-contrast black/white image
  for (int y = 0; y < qrData.height; ++y) {
    for (int x = 0; x < qrData.width; ++x) {
      uint8_t pixel = qrData.data[y * qrData.width + x];
      // Use strict threshold for pure black/white contrast
      QColor color = (pixel < 128) ? QColor(0, 0, 0) : QColor(255, 255, 255);
      qrImage.setPixelColor(x, y, color);
    }
  }

  return qrImage;
}

QImage QtSeedDisplayDialog::scaleAndPadQRImage(const QImage &qrImage, int originalWidth) {
  // Calculate optimal module size for readability
  const int maxDisplaySize = 180;
  const int minModuleSize = 4;
  const int preferredModuleSize = 8;

  int moduleSize = std::min(preferredModuleSize, maxDisplaySize / originalWidth);
  moduleSize = std::max(minModuleSize, moduleSize);

  int scaledSize = originalWidth * moduleSize;

  // Scale using nearest neighbor for crisp edges
  QImage scaledImage = qrImage.scaled(scaledSize, scaledSize,
                                     Qt::KeepAspectRatio,
                                     Qt::FastTransformation);

  // Add quiet zone padding (essential for QR code scanning)
  const int quietZone = std::max(16, moduleSize * 2);
  int finalSize = scaledSize + (quietZone * 2);

  QImage paddedImage(finalSize, finalSize, QImage::Format_RGB888);
  paddedImage.fill(Qt::white);

  // Center the QR code in the padded image
  int offsetX = (finalSize - scaledSize) / 2;
  int offsetY = (finalSize - scaledSize) / 2;

  for (int y = 0; y < scaledSize; ++y) {
    for (int x = 0; x < scaledSize; ++x) {
      paddedImage.setPixelColor(x + offsetX, y + offsetY,
                               scaledImage.pixelColor(x, y));
    }
  }

  return paddedImage;
}

void QtSeedDisplayDialog::displayQRError(const QString &message) {
  QtThemeManager &theme = QtThemeManager::instance();
  m_qrGenerated = false;
  m_qrLabel->clear();
  m_qrLabel->setText(message);
  m_qrLabel->setStyleSheet(
      m_qrLabel->styleSheet() +
      QString("color: %1; font-weight: bold; font-size: 12px;")
          .arg(theme.errorColor().name()));
}

void QtSeedDisplayDialog::displayQRWarning() {
  QtThemeManager &theme = QtThemeManager::instance();

  // Create overlay label for warning text
  QLabel *warningOverlay = new QLabel(m_qrLabel);
  warningOverlay->setText("Fallback Pattern\n(Copy text below)");
  warningOverlay->setAlignment(Qt::AlignCenter);
  warningOverlay->setStyleSheet(
      QString("background-color: rgba(255, 255, 255, 220); "
              "color: %1; font-weight: bold; font-size: 10px; "
              "border: 1px solid %2; border-radius: 4px; padding: 4px;")
          .arg(theme.warningColor().darker(150).name())
          .arg(theme.warningColor().name()));

  // Position overlay at bottom of QR label
  warningOverlay->setGeometry(5, m_qrLabel->height() - 35,
                             m_qrLabel->width() - 10, 30);
  warningOverlay->show();
}

void QtSeedDisplayDialog::onCopyToClipboard() {
  // Copy the seed phrase to clipboard
  std::ostringstream oss;
  for (size_t i = 0; i < m_seedWords.size(); ++i) {
    if (i > 0)
      oss << " ";
    oss << m_seedWords[i];
  }

  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(QString::fromStdString(oss.str()));

  // Temporarily change button text to show feedback
  QtThemeManager &theme = QtThemeManager::instance();
  m_copyButton->setText("✅ Copied!");
  m_copyButton->setStyleSheet(
      QString("QPushButton { background-color: %1; color: white; padding: 8px "
              "12px; border: none; border-radius: 4px; font-weight: bold; }")
          .arg(theme.successColor().name()));

  // Reset button after 2 seconds
  QTimer::singleShot(2000, [this]() {
    QtThemeManager &theme = QtThemeManager::instance();
    m_copyButton->setText("Copy All Words");
    m_copyButton->setStyleSheet(
        QString("QPushButton { background-color: %1; color: white; padding: "
                "8px 12px; border: none; border-radius: 4px; font-weight: "
                "bold; } QPushButton:hover { background-color: %2; }")
            .arg(theme.accentColor().name())
            .arg(theme.accentColor().darker(120).name()));
  });
}

void QtSeedDisplayDialog::onConfirmBackup() {
  if (m_confirmCheckbox->isChecked()) {
    m_userConfirmed = true;
    accept();
  }
}
