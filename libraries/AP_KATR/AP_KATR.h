/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
*/
#pragma once

#include "AP_KATR_config.h"

#if AP_KATR_ENABLED

#include <AP_CANManager/AP_CANDriver.h>
#include <SRV_Channel/SRV_Channel.h>

// KATR receiver replacement on CANBUS 1ch (standard ID 0x0F only).
class AP_KATR : public AP_CANDriver
{
public:
    AP_KATR() = default;
    CLASS_NO_COPY(AP_KATR);

    static AP_KATR *get_driver(uint8_t driver_index);
    void init(uint8_t driver_index) override;
    bool add_interface(AP_HAL::CANIface *can_iface) override;
    bool initialized() const
    {
        return _initialized;
    }

    // Snapshot final servo outputs from the main thread.
    void update();
    void set_engine_stop(bool stop);
    void set_high_speed(bool high_speed);
    void set_rough_ground(bool rough_ground);

private:
    static bool get_command(SRV_Channel::Function function, int8_t &command);
    void loop();

    AP_HAL::CANIface *_can_iface = nullptr;
    HAL_BinarySemaphore _event;
    HAL_Semaphore _sem;
    bool _initialized = false;
    char _thread_name[16];

    struct {
        int8_t steering;
        int8_t speed;
        bool motion_enabled;
        bool emergency_stop;
        bool valid;
        uint32_t updated_ms;
    } _output {};

    bool _engine_stop = true;
    bool _high_speed = false;
    bool _rough_ground = false;
};

#endif // AP_KATR_ENABLED
