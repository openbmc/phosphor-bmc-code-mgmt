class VoltageRegulator 
{
	public:
		VoltageRegulator(){}
		virtual ~VoltageRegulator() = default;
		VoltageRegulator(const VoltageRegulator&) = delete;
		VoltageRegulator& operator=(const VoltageRegulator) = delete;
		VoltageRegulator(VoltageRegulator&) = delete;
		VoltageRegulator operator=(VoltageRegulator&&) = delete;

		virtual int get_version() = 0;

		virtual void parse_file() = 0;

		virtual int fw_update() = 0;

		virtual int fw_verify() = 0;

		virtual int get_remaining_writes() = 0;

		virtual int get_crc() = 0;:

};
