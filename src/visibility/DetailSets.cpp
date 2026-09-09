// SPDX-License-Identifier: MIT
#include "cnahouse/visibility/DetailSets.hpp"

namespace cnahouse::visibility
{

    CellDetail DetailFor(ConeFlags flags, app::QualityPreset quality, float distance, int lodBias) noexcept
    {
        const bool diffuse = Has(flags, ConeFlags::Diffuse);

        CellDetail detail;
        detail.dressing = !diffuse && quality != app::QualityPreset::Low && distance <= kDressingDistance;
        // "Quality below high" is `Low` and `Medium`; `High` and `Ultra` keep it.
        const bool highEnough = quality == app::QualityPreset::High || quality == app::QualityPreset::Ultra;
        detail.micro = highEnough && distance <= kMicroDistance;
        // ...and §26.4's own arithmetic: the micro set is a subset of what dressing costs, so a
        // cell that has dropped its dressing has no business keeping its crumbs. Not stated in the
        // table, which lists the two rows independently -- but a room with individual pens drawn on
        // a desk whose books are gone is a worse picture than either rule intends.
        detail.micro = detail.micro && detail.dressing;
        detail.lodBias = lodBias + (diffuse ? 1 : 0);
        return detail;
    }

} // namespace cnahouse::visibility
