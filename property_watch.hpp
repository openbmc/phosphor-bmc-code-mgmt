/* Copyright 2022-2023 Quanta LLC*/
#pragma once

#include <string>
#include <variant>
#include <sdbusplus/bus.hpp>

using PropertyValue = std::variant<
    bool, int, double, std::string, uint16_t, uint32_t, uint64_t>;

class DbusEnvironment
{
 public:
  PropertyValue getProperty(const std::string& service, const std::string& path,
                            const std::string& interface,
                            const std::string& property);

 private:
  sdbusplus::bus_t bus = sdbusplus::bus::new_default();
};
