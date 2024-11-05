#pragma once

#include "fw-update/component/vr-i2c/i2c_vr_device.hpp"
#include "fw-update/component/interfaces/i2c/i2c_interface.hpp"

#include "xdpe152xx.hpp"

class I2CVRDevice;

namespace VR
{
	enum class VRType {
		XDPE152C4,
		ISL69260,
	};

    enum {
        VR_WARN_REMAIN_WR = 3,
    };

class VoltageRegulator
{
	public:
		static enum VRType stringToEnum(std::string& vrType);
	    int get_fw_version();
        int get_fw_full_version();
        int get_fw_active_key();
        int parse_file();
		int fw_update();
		int fw_verify();
		int get_remaining_writes(uint8_t* remain);
		int get_crc(uint32_t* sum);

	private:
		static std::map<std::string, VRType> VRTypeToString;
        i2c::I2CInterface* inf;
};


std::map<std::string, enum VRType> VoltageRegulator::VRTypeToString {
			{ "XDPE152XX", VRType::XDPE152C4 },
			{ "ISL69260", VRType::ISL69260 },
};

enum VRType VoltageRegulator::stringToEnum(std::string& vrType)
{
	return VRTypeToString[vrType];
}
std::unique_ptr<VoltageRegulator> create(std::string& vrType)
{
    switch (VoltageRegulator::stringToEnum(vrType))
    {
        case VRType::XDPE152C4:
            return XDPE152XX::create();
        case VRType::ISL69260:
    }
}
// namespace VR
}
