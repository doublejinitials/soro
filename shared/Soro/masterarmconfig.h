#ifndef MASTERARMCONFIG_H
#define MASTERARMCONFIG_H

#include "iniparser.h"

namespace Soro {

/* Holds the configuration for the master arm.
 *
 * This configuration should preferably be stored in an INI file, which
 * this class can also load.
 */
struct MasterArmConfig {

    int yawMax, yawMin, yawAdd;
    int shoulderMax, shoulderMin, shoulderAdd;
    int elbowMax, elbowMin, elbowAdd;
    int wristMax, wristMin, wristAdd;

    bool yawReverse, shoulderReverse, elbowReverse, wristReverse;

    bool load(QFile& file);
};

}

#endif // MASTERARMCONFIG_H
