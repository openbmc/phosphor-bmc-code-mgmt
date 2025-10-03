#include "gpio_controller.hpp"

#include <fstream>
#include <map>
#include <phosphor-logging/lg2.hpp>

PHOSPHOR_LOG2_USING;

GPIOGroup::GPIOGroup(std::vector<std::string> linesIn, std::vector<bool> polaritiesIn) :
    lines_(std::move(linesIn)), polarities_(std::move(polaritiesIn))
{}

GPIOGroup::~GPIOGroup()
{
    releaseAll();
}

GPIOGroup::GPIOGroup(GPIOGroup&& other) noexcept :
    lines_(std::move(other.lines_)), polarities_(std::move(other.polarities_)),
    activeBulks_(std::move(other.activeBulks_))
{}

GPIOGroup& GPIOGroup::operator=(GPIOGroup&& other) noexcept
{
    if (this != &other)
    {
        releaseAll();
        lines_ = std::move(other.lines_);
        polarities_ = std::move(other.polarities_);
        activeBulks_ = std::move(other.activeBulks_);
    }
    return *this;
}

bool GPIOGroup::muxToBMC()
{
    return mux(false);
}
bool GPIOGroup::muxToDevice()
{
    return mux(true);
}

void GPIOGroup::releaseAll()
{
    for (auto& b : activeBulks_)
    {
        if (b)
        {
            b->release();
        }
    }
    activeBulks_.clear();
}

bool GPIOGroup::mux(bool inverted)
{
    if (lines_.empty())
    {
        return true;
    }

    activeBulks_ = requestMuxGPIOs(lines_, polarities_, inverted);
    if (activeBulks_.empty())
    {
        error("Failed to mux GPIOs", "INVERTED", inverted);
        return false;
    }
    return true;
}

MuxGuard::MuxGuard(GPIOGroup& g) : group_(g)
{
    ok_ = group_.muxToBMC();
}
MuxGuard::~MuxGuard()
{
    if (!ok_)
    {
        return;
    }
    group_.releaseAll();
    group_.muxToDevice();
    group_.releaseAll();
}

std::vector<std::unique_ptr<::gpiod::line_bulk>> requestMuxGPIOs(
    const std::vector<std::string>& gpioLines,
    const std::vector<bool>& gpioPolarities, bool inverted)
{
    std::map<std::string, std::vector<std::string>> groupLineNames;
    std::map<std::string, std::vector<int>> groupValues;

    for (size_t i = 0; i < gpioLines.size(); ++i)
    {
        auto line = ::gpiod::find_line(gpioLines[i]);

        if (!line)
        {
            error("Failed to find GPIO line: {LINE}", "LINE", gpioLines[i]);
            return {};
        }

        if (line.is_used())
        {
            error("GPIO line {LINE} was still used", "LINE", gpioLines[i]);
            return {};
        }

        std::string chipName = line.get_chip().name();
        groupLineNames[chipName].push_back(gpioLines[i]);
        groupValues[chipName].push_back(gpioPolarities[i] ^ inverted ? 1 : 0);
    }

    std::vector<std::unique_ptr<::gpiod::line_bulk>> lineBulks;
    ::gpiod::line_request config{"", ::gpiod::line_request::DIRECTION_OUTPUT,
                                 0};

    for (auto& [chipName, lineNames] : groupLineNames)
    {
        ::gpiod::chip chip(chipName);
        std::vector<::gpiod::line> lines;

        for (size_t i = 0; i < lineNames.size(); ++i)
        {
            const auto& name = lineNames[i];
            auto line = chip.find_line(name);

            if (!line)
            {
                error("Failed to get {LINE} from chip {CHIP}", "LINE", name,
                      "CHIP", chipName);
                return {};
            }

            lg2::info("Requesting chip {CHIP}, GPIO line {LINE} to {VALUE}",
                      "CHIP", chip.name(), "LINE", line.name(), "VALUE",
                      groupValues[chipName][i]);

            lines.push_back(std::move(line));
        }

        auto lineBulk = std::make_unique<::gpiod::line_bulk>(lines);

        if (!lineBulk)
        {
            error("Failed to create line bulk for chip={CHIP}", "CHIP",
                  chipName);
            return {};
        }

        lineBulk->request(config, groupValues[chipName]);

        lineBulks.push_back(std::move(lineBulk));
    }

    return lineBulks;
}
