#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QTimer>
#include <QMouseEvent>
#include <QPoint>
#include <QPainter>
#include <QPaintEvent>
#include <QGraphicsDropShadowEffect>

class QtSuccessDialog : public QDialog {
    Q_OBJECT

public:
    explicit QtSuccessDialog(const QString& username, QWidget* parent = nullptr);
    ~QtSuccessDialog();

private slots:
    void onOkClicked();
    void onAutoDismiss();

private:
    void setupUI();
    void setupConnections();
    void applyTheme();
    void setupAutoDismiss();
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

    // UI Components
    QFrame* m_card;
    QLabel* m_successIcon;
    QLabel* m_titleLabel;
    QLabel* m_messageLabel;
    QLabel* m_infoLabel;
    QPushButton* m_okButton;
    
    // Layout
    QVBoxLayout* m_mainLayout;
    QVBoxLayout* m_cardLayout;
    QHBoxLayout* m_buttonLayout;
    
    // Data
    QString m_username;
    QTimer* m_autoDismissTimer;
    QRect m_screenGeometry;
    int m_dialogWidth;
    int m_dialogHeight;
    int m_dialogX;
    int m_dialogY;
    QPoint m_dragPosition;
    
    // Constants
    static constexpr int AUTO_DISMISS_DELAY = 5000; // 5 seconds
};