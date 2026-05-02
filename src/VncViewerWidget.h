#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QWidget>

class VncClientSession;
class QEvent;
class QWheelEvent;

class VncViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit VncViewerWidget(QWidget* parent = nullptr);

    void setSession(VncClientSession* session);

signals:
    void pointerEvent(int x, int y, int buttonMask);
    void keyEvent(quint32 keySym, bool down);

public slots:
    void setFrame(const QImage& image);
    void setRemoteSize(const QSize& size);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    QRect displayRect() const;
    void updateLocalCursorVisibility(const QPoint& localPos);
    QPoint toRemotePos(const QPoint& localPos) const;
    int buttonMaskFromEvent(Qt::MouseButtons buttons) const;
    quint32 toKeysym(QKeyEvent* event) const;

    VncClientSession* m_session = nullptr;
    QImage m_frame;
    QSize m_remoteSize;
    QElapsedTimer m_pointerThrottle;
};
