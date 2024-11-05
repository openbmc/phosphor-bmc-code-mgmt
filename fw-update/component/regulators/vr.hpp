#include "fw-update/component/vr-i2c/i2c_vr_device.hpp"

#include <string>
#include <map>

class I2CVRDevice;

enum VRType {
	XDPE152XX,
	ISL69260,
};

std::map<string, VRType> VRTypeToString = {
	{ "XDPE152XX", XDPE152XX },
	{ "ISL69260", ISL69260 },
};

enum VRType stringToEnum(std::string vrtype)
{
	return VRTypeToString[vrtype];
}

	class VoltageRegulator
	{
		public:
		VoltageRegulator(I2CVRDevice* parent);
		virtual ~VoltageRegulator() = default;
		VoltageRegulator(const VoltageRegulator&) = delete;
		VoltageRegulator& operator=(const VoltageRegulator) = delete;
		VoltageRegulator(VoltageRegulator&) = delete;
		VoltageRegulator operator=(VoltageRegulator&&) = delete;
		VoltageRegulator(const VoltageRegulator&&) = delete;

		virtual int get_version() = 0;

		virtual void parse_file() = 0;

		virtual int fw_update() = 0;

		virtual int fw_verify() = 0;

		virtual int get_remaining_writes() = 0;

		virtual int get_crc() = 0;

		I2CVRDevice* parent;
};


