#ifndef AUDIOSERVER_H
#define AUDIOSERVER_H

#include <QObject>

#include "socketaddress.h"
#include "soro_global.h"
#include "logger.h"
#include "mediaserver.h"

namespace Soro {
namespace Rover {

class AudioServer: public MediaServer {
    Q_OBJECT
public:

    explicit AudioServer(int mediaId, SocketAddress host, Logger *log = 0, QObject *parent = 0);

    void start(QString deviceName, AudioFormat format);

    AudioFormat getAudioFormat() const;

private:
    AudioFormat _format;
    QString _audioDevice;

protected:
    /* Begins streaming video to the provided address.
     * This will fail if the stream is not in WaitingState
     */
    void constructChildArguments(QStringList& outArgs, SocketAddress host, SocketAddress address, quint16 ipcPort) Q_DECL_OVERRIDE;

    void onStreamStoppedInternal() Q_DECL_OVERRIDE;

    void constructStreamingMessage(QDataStream& stream) Q_DECL_OVERRIDE;
};

} // namespace Rover
} // namespace Soro

#endif // AUDIOSERVER_H