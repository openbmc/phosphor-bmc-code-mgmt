#include "update_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>

PHOSPHOR_LOG2_USING;

int main()
{
    try
    {
        auto ctx = std::make_shared<sdbusplus::async::context>();
        phosphor::software::UpdateManager manager(*ctx);
        // For now, just run the context, no real logic yet
        ctx->run();
    }
    catch (const std::exception& e)
    {
        error("Error while starting code-update-manager: {ERROR}", "ERROR",
              e.what());
        return 1;
    }
    return 0;
}
