#pragma once

#include <QTimer>
#include <QColor>
#include <QHideEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QShowEvent>
#include <QWidget>

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
