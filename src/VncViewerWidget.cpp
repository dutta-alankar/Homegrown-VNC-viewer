#include "VncViewerWidget.h"

#include <algorithm>

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

extern "C" {
#include <rfb/keysym.h>
}

VncViewerWidget::VncViewerWidget(QWidget* parent)
    : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void VncViewerWidget::setSession(VncClientSession* session) {
    m_session = session;
}

void VncViewerWidget::setFrame(const QImage& image) {
    m_frame = image;
    if (m_remoteSize.isEmpty()) {
        m_remoteSize = image.size();
    }
    update();
}

void VncViewerWidget::setRemoteSize(const QSize& size) {
    if (size.isValid()) {
        m_remoteSize = size;
    }
}

void VncViewerWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (m_frame.isNull()) {
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, "Connecting...");
        return;
    }

    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(displayRect(), m_frame);
}

void VncViewerWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    update();
}

void VncViewerWidget::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    unsetCursor();
}

void VncViewerWidget::mousePressEvent(QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint pos = event->position().toPoint();
#else
    const QPoint pos = event->pos();
#endif
    updateLocalCursorVisibility(pos);
    emit pointerEvent(toRemotePos(pos).x(), toRemotePos(pos).y(), buttonMaskFromEvent(event->buttons()));
}

void VncViewerWidget::mouseReleaseEvent(QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint pos = event->position().toPoint();
#else
    const QPoint pos = event->pos();
#endif
    updateLocalCursorVisibility(pos);
    emit pointerEvent(toRemotePos(pos).x(), toRemotePos(pos).y(), buttonMaskFromEvent(event->buttons()));
}

void VncViewerWidget::mouseMoveEvent(QMouseEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint pos = event->position().toPoint();
#else
    const QPoint pos = event->pos();
#endif
    updateLocalCursorVisibility(pos);
    // Throttle move-only events to ~60fps to avoid flooding the server.
    // Always send immediately when buttons are held (drag operations).
    const bool buttonsHeld = event->buttons() != Qt::NoButton;
    if (buttonsHeld || !m_pointerThrottle.isValid() || m_pointerThrottle.elapsed() >= 16) {
        emit pointerEvent(toRemotePos(pos).x(), toRemotePos(pos).y(), buttonMaskFromEvent(event->buttons()));
        m_pointerThrottle.restart();
    }
}

void VncViewerWidget::wheelEvent(QWheelEvent* event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint pos = event->position().toPoint();
#else
    const QPoint pos = event->pos();
#endif
    updateLocalCursorVisibility(pos);

    const QPoint remotePos = toRemotePos(pos);
    const int baseMask = buttonMaskFromEvent(event->buttons());
    const QPoint delta = event->angleDelta();

    if (delta.y() > 0) {
        emit pointerEvent(remotePos.x(), remotePos.y(), baseMask | 8);
        emit pointerEvent(remotePos.x(), remotePos.y(), baseMask);
    } else if (delta.y() < 0) {
        emit pointerEvent(remotePos.x(), remotePos.y(), baseMask | 16);
        emit pointerEvent(remotePos.x(), remotePos.y(), baseMask);
    }

    if (delta.x() > 0) {
        emit pointerEvent(remotePos.x(), remotePos.y(), baseMask | 32);
        emit pointerEvent(remotePos.x(), remotePos.y(), baseMask);
    } else if (delta.x() < 0) {
        emit pointerEvent(remotePos.x(), remotePos.y(), baseMask | 64);
        emit pointerEvent(remotePos.x(), remotePos.y(), baseMask);
    }

    event->accept();
}

void VncViewerWidget::keyPressEvent(QKeyEvent* event) {
    const quint32 sym = toKeysym(event);
    if (sym != 0) {
        emit keyEvent(sym, true);
    }
}

void VncViewerWidget::keyReleaseEvent(QKeyEvent* event) {
    const quint32 sym = toKeysym(event);
    if (sym != 0) {
        emit keyEvent(sym, false);
    }
}

QRect VncViewerWidget::displayRect() const {
    if (m_frame.isNull()) {
        return rect();
    }

    QSize scaled = m_frame.size();
    scaled.scale(size(), Qt::KeepAspectRatio);
    return QRect((width() - scaled.width()) / 2,
                 (height() - scaled.height()) / 2,
                 scaled.width(),
                 scaled.height());
}

void VncViewerWidget::updateLocalCursorVisibility(const QPoint& localPos) {
    if (!m_frame.isNull() && displayRect().contains(localPos)) {
        setCursor(Qt::BlankCursor);
    } else {
        unsetCursor();
    }
}

QPoint VncViewerWidget::toRemotePos(const QPoint& localPos) const {
    if (!m_remoteSize.isValid() || width() <= 0 || height() <= 0) {
        return localPos;
    }

    const QRect target = displayRect();
    const int clampedX = qBound(target.left(), localPos.x(), target.right());
    const int clampedY = qBound(target.top(), localPos.y(), target.bottom());
    const int relX = clampedX - target.left();
    const int relY = clampedY - target.top();

    const int x = qBound(0, relX * m_remoteSize.width() / std::max(1, target.width()), m_remoteSize.width() - 1);
    const int y = qBound(0, relY * m_remoteSize.height() / std::max(1, target.height()), m_remoteSize.height() - 1);
    return {x, y};
}

int VncViewerWidget::buttonMaskFromEvent(Qt::MouseButtons buttons) const {
    int mask = 0;
    if (buttons & Qt::LeftButton) {
        mask |= 1;
    }
    if (buttons & Qt::MiddleButton) {
        mask |= 2;
    }
    if (buttons & Qt::RightButton) {
        mask |= 4;
    }
    return mask;
}

quint32 VncViewerWidget::toKeysym(QKeyEvent* event) const {
    switch (event->key()) {
    case Qt::Key_Backspace:
        return XK_BackSpace;
    case Qt::Key_Tab:
        return XK_Tab;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return XK_Return;
    case Qt::Key_Escape:
        return XK_Escape;
    case Qt::Key_Insert:
        return XK_Insert;
    case Qt::Key_Delete:
        return XK_Delete;
    case Qt::Key_Home:
        return XK_Home;
    case Qt::Key_End:
        return XK_End;
    case Qt::Key_PageUp:
        return XK_Page_Up;
    case Qt::Key_PageDown:
        return XK_Page_Down;
    case Qt::Key_Left:
        return XK_Left;
    case Qt::Key_Up:
        return XK_Up;
    case Qt::Key_Right:
        return XK_Right;
    case Qt::Key_Down:
        return XK_Down;
    case Qt::Key_F1:
        return XK_F1;
    case Qt::Key_F2:
        return XK_F2;
    case Qt::Key_F3:
        return XK_F3;
    case Qt::Key_F4:
        return XK_F4;
    case Qt::Key_F5:
        return XK_F5;
    case Qt::Key_F6:
        return XK_F6;
    case Qt::Key_F7:
        return XK_F7;
    case Qt::Key_F8:
        return XK_F8;
    case Qt::Key_F9:
        return XK_F9;
    case Qt::Key_F10:
        return XK_F10;
    case Qt::Key_F11:
        return XK_F11;
    case Qt::Key_F12:
        return XK_F12;
    case Qt::Key_Shift:
        return XK_Shift_L;
    case Qt::Key_Control:
        return XK_Control_L;
    case Qt::Key_Alt:
        return XK_Alt_L;
    default:
        break;
    }

    const QString text = event->text();
    if (text.isEmpty()) {
        return 0;
    }
    return static_cast<quint32>(text.at(0).unicode());
}
