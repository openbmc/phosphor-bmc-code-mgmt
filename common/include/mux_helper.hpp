#pragma once

#include <sdbusplus/async.hpp>

#include <string>
#include <tuple>
#include <vector>

sdbusplus::async::task<std::tuple<std::vector<std::string>, std::vector<bool>>>
    getMuxGPIOs(sdbusplus::async::context& ctx, std::string service,
                sdbusplus::object_path path, std::string configIface);
