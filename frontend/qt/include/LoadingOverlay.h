#pragma once

#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QShowEvent>
#include <QHideEvent>
#include <QPaintEvent>

class LoadingOverlay : public QWidget {
    Q_OBJECT
public:
    explicit LoadingOverlay(QWidget* parent);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QTimer* m_timer;
    int m_angle;
};
