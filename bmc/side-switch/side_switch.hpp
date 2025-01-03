#pragma once

#include <sdbusplus/bus.hpp>

/** @brief Determine if a side switch is needed
 *
 *  @param[in] bus       - The Dbus bus object
 *  @return True if side switch needed, false otherwise
 */
bool sideSwitchNeeded(sdbusplus::bus_t& bus);

/** @brief Power off the system
 *
 *  @param[in] bus       - The Dbus bus object
 *  @return True if chassis off success, false otherwise
 */
bool powerOffSystem(sdbusplus::bus_t& bus);

/** @brief Configure BMC to auto power on the host after reboot
 *
 *  @param[in] bus       - The Dbus bus object
 *  @return True if policy set correctly, false otherwise
 */
bool setAutoPowerRestart(sdbusplus::bus_t& bus);

/** @brief Reboot the BMC
 *
 *  @param[in] bus       - The Dbus bus object
 *  @return True if reboot request had no error, false otherwise
 */
bool rebootTheBmc(sdbusplus::bus_t& bus);
