#pragma once

#include <sdbusplus/bus.hpp>

/** @brief Determine if a side switch is needed
 *
 *  @param[in] bus       - The Dbus bus object
 *  @return True if side switch needed, false otherwise
 */
bool sideSwitchNeeded(sdbusplus::bus::bus& bus);

/** @brief Power off the system
 *
 *  @param[in] bus       - The Dbus bus object
 *  @return True if chassis off success, false otherwise
 */
bool powerOffSystem(sdbusplus::bus::bus& bus);
