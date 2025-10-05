#pragma once

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

// Forward declarations
class QtThemeManager;

class QtExpandableWalletCard : public QFrame {
  Q_OBJECT

public:
  explicit QtExpandableWalletCard(QtThemeManager *themeManager,
                                   QWidget *parent = nullptr);

  // Configuration
  void setCryptocurrency(const QString &name, const QString &symbol,
                         const QString &logoText);
  void setBalance(const QString &balance);
  void setTransactionHistory(const QString &historyHtml);
  void applyTheme();

  // Access to buttons for signal connections
  QPushButton *sendButton() const { return m_sendButton; }
  QPushButton *receiveButton() const { return m_receiveButton; }

signals:
  void sendRequested();
  void receiveRequested();

private slots:
  void toggleExpanded();

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  void setupUI();
  void updateStyles();

  // Theme
  QtThemeManager *m_themeManager;

  // UI Components
  QWidget *m_collapsedHeader;
  QLabel *m_cryptoLogo;
  QLabel *m_cryptoName;
  QLabel *m_balanceLabel;
  QLabel *m_expandIndicator;

  QWidget *m_expandedContent;
  QPushButton *m_sendButton;
  QPushButton *m_receiveButton;
  QLabel *m_historyTitleLabel;
  QTextEdit *m_historyText;

  // State
  bool m_isExpanded;
  QString m_cryptoSymbol;
};
