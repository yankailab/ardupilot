/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
*/

#include "AP_KATR.h"

#if AP_KATR_ENABLED

#include <AP_CANManager/AP_CANManager.h>
#include <AP_Math/AP_Math.h>
#include <GCS_MAVLink/GCS.h>

extern const AP_HAL::HAL& hal;

AP_KATR *AP_KATR::get_driver(uint8_t driver_index)
{
    if (AP::can().get_driver_type(driver_index) != AP_CAN::Protocol::KATR) {
        return nullptr;
    }
    return static_cast<AP_KATR *>(AP::can().get_driver(driver_index));
}

bool AP_KATR::add_interface(AP_HAL::CANIface *can_iface)
{
    if (_can_iface != nullptr || can_iface == nullptr || !can_iface->is_initialized()) {
        return false;
    }
    if (!can_iface->set_event_handle(&_event)) {
        return false;
    }
    _can_iface = can_iface;
    return true;
}

void AP_KATR::init(uint8_t driver_index)
{
    if (_initialized) {
        return;
    }
    if (_can_iface == nullptr) {
        GCS_SEND_TEXT(MAV_SEVERITY_ERROR, "KATR: no CAN interface");
        return;
    }
    hal.util->snprintf(_thread_name, sizeof(_thread_name), "KATR_%u", unsigned(driver_index));
    _initialized = hal.scheduler->thread_create(FUNCTOR_BIND_MEMBER(&AP_KATR::loop, void),
                   _thread_name, 2048, AP_HAL::Scheduler::PRIORITY_CAN, 0);
    if (!_initialized) {
        GCS_SEND_TEXT(MAV_SEVERITY_ERROR, "KATR: thread creation failed");
    }
}

bool AP_KATR::get_command(SRV_Channel::Function function, int8_t &command)
{
    command = 0;
    uint8_t index;
    if (!SRV_Channels::find_channel(function, index)) {
        return false;
    }
    const SRV_Channel *channel = SRV_Channels::srv_channel(index);
    const uint16_t pwm = channel->get_output_pwm();
    const uint16_t trim = channel->get_trim();
    const uint16_t minimum = channel->get_output_min();
    const uint16_t maximum = channel->get_output_max();
    if (pwm == 0 || minimum >= trim || trim >= maximum) {
        return false;
    }
    // Use final PWM so output limits and SERVOx_REVERSED apply exactly once.
    const float span = pwm >= trim ? maximum - trim : trim - minimum;
    command = int8_t(roundf(constrain_float((int32_t(pwm) - trim) / span, -1.0f, 1.0f) * 68.0f));
    return true;
}

void AP_KATR::update()
{
    WITH_SEMAPHORE(_sem);
    const bool steering_valid = get_command(SRV_Channel::k_steering, _output.steering);
    const bool speed_valid = get_command(SRV_Channel::k_throttle, _output.speed);
    // KATR RadioCh2 is negative for forward travel.
    _output.speed = -_output.speed;
    _output.motion_enabled = steering_valid && speed_valid && hal.util->get_soft_armed() &&
                             hal.util->safety_switch_state() != AP_HAL::Util::SAFETY_DISARMED;
    _output.emergency_stop = SRV_Channels::get_emergency_stop();
    _output.updated_ms = AP_HAL::millis();
    _output.valid = true;
}

void AP_KATR::set_engine_stop(bool stop)
{
    WITH_SEMAPHORE(_sem);
    _engine_stop = stop;
}

void AP_KATR::set_high_speed(bool high_speed)
{
    WITH_SEMAPHORE(_sem);
    _high_speed = high_speed;
}

void AP_KATR::set_rough_ground(bool rough_ground)
{
    WITH_SEMAPHORE(_sem);
    _rough_ground = rough_ground;
}

void AP_KATR::loop()
{
    uint32_t last_send_ms = AP_HAL::millis();
    while (true) {
        hal.scheduler->delay(1);
        const uint32_t now_ms = AP_HAL::millis();
        if (now_ms - last_send_ms < 30) {
            continue;
        }
        last_send_ms = now_ms;

        AP_HAL::CANFrame frame {};
        frame.id = 0x0F;
        frame.dlc = 8;
        {
            WITH_SEMAPHORE(_sem);
            // Keep sending a stop if the main loop ceases to update outputs.
            const bool stale = !_output.valid || now_ms - _output.updated_ms > 200;
            const bool stop = _engine_stop || _output.emergency_stop || stale;
            const bool enabled = _output.motion_enabled && !stop && hal.util->get_soft_armed() &&
                                 hal.util->safety_switch_state() != AP_HAL::Util::SAFETY_DISARMED;
            frame.data[0] = enabled ? uint8_t(_output.steering) : 0;
            frame.data[1] = enabled ? uint8_t(_output.speed) : 0;
            frame.data[2] = uint8_t(stop ? 92 : -92);
            frame.data[4] = uint8_t(_high_speed ? -92 : 92);
            frame.data[5] = uint8_t(_rough_ground ? -92 : 92);
        }

        bool read_select = false;
        bool write_select = true;
        const uint64_t deadline_us = AP_HAL::micros64() + 1000;
        if (_can_iface->select(read_select, write_select, &frame, deadline_us) && write_select) {
            _can_iface->send(frame, deadline_us, AP_HAL::CANIface::AbortOnError);
        }

        // This interface is transmit-only; discard traffic without interpreting it.
        AP_HAL::CANFrame received_frame;
        uint64_t timestamp_us;
        AP_HAL::CANIface::CanIOFlags flags;
        for (uint8_t i = 0; i < 16; i++) {
            if (_can_iface->receive(received_frame, timestamp_us, flags) <= 0) {
                break;
            }
        }
    }
}

#endif // AP_KATR_ENABLED
