// SPDX-License-Identifier: MIT
#include "cnahouse/rendering/RenderTier.hpp"

#include "cnahouse/util/Log.hpp"

namespace cnahouse::rendering
{

    bool RenderTier::FallBackToS(std::string_view reason)
    {
        if (active_ == Tier::S)
        {
            return false; // already there; the caller must not log again
        }
        active_ = Tier::S;
        reason_ = std::string(reason);
        util::Log::Warn(util::LogCat::Rendering,
                        "Tier E is unavailable and the renderer has fallen back to Tier S, which is "
                        "complete by design (ADR-0003): {}",
                        reason_);
        return true;
    }

} // namespace cnahouse::rendering
