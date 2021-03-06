#ifndef SORO_MISSIONCONTROL_CAMERAWIDGET_H
#define SORO_MISSIONCONTROL_CAMERAWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QTimerEvent>
#include <QResizeEvent>

#include <Qt5GStreamer/QGst/Ui/VideoWidget>
#include <Qt5GStreamer/QGst/Pipeline>
#include <Qt5GStreamer/QGst/Element>
#include <Qt5GStreamer/QGst/ElementFactory>
#include <Qt5GStreamer/QGst/Bin>
#include <Qt5GStreamer/QGst/Bus>
#include <Qt5GStreamer/QGlib/RefPointer>
#include <Qt5GStreamer/QGlib/Error>
#include <Qt5GStreamer/QGlib/Connect>
#include <Qt5GStreamer/QGst/Message>

#include "libsoro/socketaddress.h"
#include "libsoro/enums.h"
#include "libsoro/videoformat.h"

#include "soro_missioncontrol_global.h"

namespace Ui {
class CameraWidget;
}

namespace Soro {
namespace MissionControl {

/* UI control for playing a UDP video stream using the
 * gstreamer-1.0 backend
 */
class LIBSOROMC_EXPORT CameraWidget : public QWidget {
    Q_OBJECT
public:
    explicit CameraWidget(QWidget *parent = 0);
    ~CameraWidget();

    enum Pattern {
        Pattern_SMPTE = 0,
        Pattern_Snow,
        Pattern_Black,
        Pattern_White,
        Pattern_Red,
        Pattern_Green,
        Pattern_Blue,
        Pattern_Checkers1,
        Pattern_Checkers2,
        Pattern_Checkers4,
        Pattern_Checkers8,
        Pattern_Circular,
        Pattern_Blink,
        Pattern_SMPTE75,
        Pattern_ZonePlate,
        Pattern_Gamut,
        Pattern_ChromaZonePlate,
        Pattern_Solid,
        Pattern_Ball,
        Pattern_SMPTE100,
        Pattern_Bar,
        Pattern_Pinwheel,
        Pattern_Spokes,
        Pattern_Gradient,
        Pattern_Colors
    };

    /* Configure the widget to receive a video stream from a UDP socket. If succesful,
     * the widget should start playing the stream immediately.
     */
    void play(SocketAddress address, VideoFormat format);

    /* Stops video playback, and displays they reason why
     * if one is provided.
     */
    void stop(QString reason = "", CameraWidget::Pattern pattern=CameraWidget::Pattern::Pattern_Snow);

    /* Set to false to disable the text overlay feature
     */
    void showText(bool show);

    /* Set to false to hide the video label
     */
    void showLabel(bool show);

    QString getCameraName();
    void setCameraName(QString name);

    bool isPlaying();

private:
    Ui::CameraWidget *ui;
    QGst::PipelinePtr _pipeline;
    bool _isPlaying = false;
    bool _showLabel = true;
    bool _showText = true;
    VideoFormat _videoFormat;

    QGst::ElementPtr createSink();
    void resetPipeline();

private slots:
    /* Recieves messages from the gstreamer pipeline bus
     */
    void onBusMessage(const QGst::MessagePtr & message);

protected:
    void resizeEvent(QResizeEvent *e);
    void adjustVideoSize();

signals:
    /* Emitted when the widget receives an end-of-stream message
     */
    void eosMessage();

    /* Emitted when the widget encounters a playback error
     */
    void error();
};

} // namespace MissionControl
} // namespace Soro

#endif // SORO_MISSIONCONTROL_CAMERAWIDGET_H
