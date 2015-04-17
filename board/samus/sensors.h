#ifndef __CROS_EC_SENSORS_H
#define __CROS_EC_SENSORS_H
#include "sensors.wrap"

struct motion_sensor_count {
	SENSOR_LIST(EXPAND_AS_STRUCT)
};
#define MOTION_SENSOR_COUNT sizeof(struct motion_sensor_count)

/* Declare enums here */
/* Create chip enum */
enum motion_sensor_chips {
	MOTION_SENSOR_CHIPS(CREATE_CHIP_ENUMS)
	NUM_MOTION_SENSOR_CHIPS
};

/* Create enums for each of the sensors on the chips */
MOTION_SENSOR_CHIPS(CREATE_SENSOR_ENUMS)

#endif /* __CROS_EC_SENSORS_H */
