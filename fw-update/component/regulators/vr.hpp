#pragma once

#include "fw-update/component/interfaces/i2c/i2c_interface.hpp"

#include <map>

namespace VR
{
	enum class VRType {
		XDPE152XX,
        XDPE192XX,
		ISL69260,
	};

    enum {
        VR_WARN_REMAIN_WR = 3,
    };

class VoltageRegulator
{
	public:
	    virtual ~VoltageRegulator();
        virtual int get_fw_version() = 0;
        int get_fw_full_version();
        int get_fw_active_key();
		int fw_update();
		int get_remaining_writes(uint8_t* remain);
        static enum VRType stringToEnum(std::string& vrType);

	private:
		static std::map<std::string, VRType> VRTypeToString;
        i2c::I2CInterface* inf;
};


std::map<std::string, enum VRType> VoltageRegulator::VRTypeToString {
			{ "XDPE152XX", VRType::XDPE152XX },
            { "XDPE192XX", VRType::XDPE152XX },
            { "XDPE1A2XX", VRType::XDPE152XX},
			{ "ISL69260", VRType::ISL69260 },
};

std::unique_ptr<VoltageRegulator> create(std::string& vrType, uint8_t bus, uint8_t address);

// namespace VR
}
