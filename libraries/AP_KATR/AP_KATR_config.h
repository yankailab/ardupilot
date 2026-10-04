#pragma once

#include <AP_HAL/AP_HAL_Boards.h>
#include <AP_Vehicle/AP_Vehicle_Type.h>

#ifndef AP_KATR_ENABLED
#define AP_KATR_ENABLED (HAL_CANMANAGER_ENABLED && HAL_NUM_CAN_IFACES && HAL_PROGRAM_SIZE_LIMIT_KB > 1024 && APM_BUILD_TYPE(APM_BUILD_Rover))
#endif
