#ifndef RESEARCHROVERPROCESS_H
#define RESEARCHROVERPROCESS_H

#include <QObject>

#include "libsoro/channel.h"
#include "libsoro/mbedchannel.h"
#include "libsoro/gpsserver.h"
#include "libsoro/audioserver.h"
#include "libsoro/nmeamessage.h"
#include "libsoro/videoserver.h"

namespace Soro {
namespace Research {

class ResearchRoverProcess : public QObject
{
    Q_OBJECT
private:
    /* Connects to mission control for command and status communication
     */
    Channel *_driveChannel = NULL;
    Channel *_sharedChannel = NULL;

    /* Interfaces with the mbed controlling the drive system
     */
    MbedChannel *_mbed = NULL;

    /* Provides GPS coordinates back to mission control
     */
    GpsServer *_gpsServer = NULL;

    /* Provides audio back to mission control
     */
    AudioServer *_audioServer = NULL;

    /* Handles video streaming from each individual camera
     */
    VideoServer *_stereoRCameraServer = NULL;
    QString _stereoRCameraDevice;
    VideoServer *_stereoLCameraServer = NULL;
    QString _stereoLCameraDevice;
    VideoServer *_aux1CameraServer = NULL;
    QString _aux1CameraDevice;

private slots:
    void init();
    void sendSystemStatusMessage();
    void sharedChannelStateChanged(Channel* channel, Channel::State state);
    void driveChannelStateChanged(Channel* channel, Channel::State state);
    void mbedChannelStateChanged(MbedChannel* channel, MbedChannel::State state);
    void driveChannelMessageReceived(Channel* channel, const char* message, Channel::MessageSize size);
    void sharedChannelMessageReceived(Channel* channel, const char* message, Channel::MessageSize size);
    void mbedMessageReceived(MbedChannel* channel, const char* message, int size);
    void gpsUpdate(NmeaMessage message);

public:
    explicit ResearchRoverProcess(QObject *parent = 0);
    ~ResearchRoverProcess();

signals:

public slots:
};


} // namespace Research
} // namespace Soro

#endif // RESEARCHROVERPROCESS_H