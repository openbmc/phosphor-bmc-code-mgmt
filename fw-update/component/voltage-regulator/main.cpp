
#include "vr_code_updater.hpp"

int main(){

	boost::asio::io_context io;
	bool dryRun = true;
	auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

	VRCodeUpdater vrcu(systemBus, io, dryRun);

	io.run();
	return 0;
}
