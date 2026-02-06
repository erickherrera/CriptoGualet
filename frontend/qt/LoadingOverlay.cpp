#include "LoadingOverlay.h"

LoadingOverlay::LoadingOverlay(QWidget* parent) 
    : QWidget(parent), m_timer(new QTimer(this)), m_angle(0) 
{
    if (parent) {
        resize(parent->size());
        parent->installEventFilter(this);
    }
    setAttribute(Qt::WA_TransparentForMouseEvents, false); // Block inputs
    
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_angle = (m_angle + 10) % 360;
        update();
    });

    hide();
}

void LoadingOverlay::showEvent(QShowEvent* event) {
    if (parentWidget()) resize(parentWidget()->size());
    m_angle = 0;
    if (m_timer) m_timer->start(30);
    QWidget::showEvent(event);
}

void LoadingOverlay::hideEvent(QHideEvent* event) {
    if (m_timer) m_timer->stop();
    QWidget::hideEvent(event);
}

bool LoadingOverlay::eventFilter(QObject* obj, QEvent* event) {
    if (obj == parentWidget() && event->type() == QEvent::Resize) {
        if (QWidget* widget = qobject_cast<QWidget*>(obj)) {
            resize(widget->size());
        }
    }
    return QWidget::eventFilter(obj, event);
}

void LoadingOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Dark overlay
    p.fillRect(rect(), QColor(0, 0, 0, 100)); // Semi-transparent black

    p.translate(width() / 2, height() / 2);
    p.rotate(m_angle);

    // Draw abstract spinner (3 arcs)
    QPen pen;
    pen.setWidth(4);
    pen.setCapStyle(Qt::RoundCap);
    
    int r = 24;
    
    pen.setColor(QColor(255, 255, 255, 240));
    p.setPen(pen);
    p.drawArc(-r, -r, 2*r, 2*r, 0, 100 * 16);
    
    pen.setColor(QColor(255, 255, 255, 160));
    p.setPen(pen);
    p.drawArc(-r, -r, 2*r, 2*r, 120 * 16, 100 * 16);

    pen.setColor(QColor(255, 255, 255, 80));
    p.setPen(pen);
    p.drawArc(-r, -r, 2*r, 2*r, 240 * 16, 100 * 16);
}