/* Copyright 2022-2023 Quanta LLC*/
#include "property_watch.hpp"

#include <exception>
#include <string>

#include <sdbusplus/bus.hpp>
#include <phosphor-logging/lg2.hpp>

PropertyValue DbusEnvironment::getProperty(const std::string& service,
                                      const std::string& path,
                                      const std::string& interface,
                                      const std::string& property) {
  PropertyValue value;

  auto dbusCall = bus.new_method_call(service.c_str(), path.c_str(),
                                      "org.freedesktop.DBus.Properties", "Get");
  dbusCall.append(interface, property);

  try
  {
    auto responds = bus.call(dbusCall);
    responds.read(value);
  }
  catch (const std::exception& e)
  {
    lg2::error("{ERRMSG}", "ERRMSG", e.what());
  }

  return value;
}
