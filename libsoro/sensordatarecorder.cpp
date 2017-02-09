/*
 * Copyright 2017 The University of Oklahoma.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sensordatarecorder.h"

#define LOG_TAG "SensorDataRecorder"

const char *TAGS[] =
{
    "!", // Wheeldata 1
    "@", // Wheeldata 2
    "#", // Wheeldata 3
    "$", // Wheeldata 4
    "%", // Wheeldata 5
    "^", // Wheeldata 6
    "+!", // IMUdata 1 X
    "+@", // IMUdata 1 Y
    "+#", // IMUdata 1 Z
    "~!", // IMUdata 2 X
    "~@", // IMUdata 2 Y
    "~#" // IMUdata 2 Z
};

#define DATA_FOOTER '&'

namespace Soro {

SensorDataRecorder::SensorDataRecorder(QObject *parent) : AbstractDataRecorder(LOG_TAG, parent) {

}

void SensorDataRecorder::newData(const char* data, int len) {
    _buffer.append(data, len);
    parseBuffer();
}

void SensorDataRecorder::parseBuffer() {
    if (_buffer.length() == 0) return;

    for (int i = 0; i < 12; i++) {
        if (_buffer.startsWith(TAGS[i])) {
            if (parseNext(reinterpret_cast<const DataTag&>(i), strlen(TAGS[i]))) {
                parseBuffer();
            }
            return;
        }
    }

    // Unknown start token in buffer, remove chars until one is recognized or
    // the buffer is empty
    LOG_E(LOG_TAG, "Invalid token, buffer contents: " + QString(_buffer));
    _buffer = _buffer.remove(0, 1);
    parseBuffer();
}

bool SensorDataRecorder::parseNext(DataTag tag, int start) {
    if (start >= _buffer.size()) return false;
    int end = start;
    while (QChar(_buffer.at(end)) != DATA_FOOTER) {
        end++;
        if (end >= _buffer.size()) return false;
    }

    bool ok;
    float value = _buffer.mid(start, end - start).toFloat(&ok);
    _buffer = _buffer.remove(0, end + 1);
    if (!ok) {
        LOG_W(LOG_TAG, "Received invalid data, discarding");
        return true;
    }


    // Append this data to the logfile;
    if (_fileStream) {
        addTimestamp();
        *_fileStream << reinterpret_cast<quint32&>(tag) << value;
    }

    emit dataParsed(tag, value);
    return true;
}

} // namespace Soro
