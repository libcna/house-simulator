// SPDX-License-Identifier: MIT
//
// `HOUSE-00696`. `docs/visibility.md` §3 lists every constant §25 has, its value, and the header
// that owns it. **A table of numbers copied out of code is a table that goes stale**, and this
// project already checks its other copied tables that way -- `room_schedule`, `window_schedule`
// and `budget_report` all compare a document against the thing it describes. This does the same
// for the one table a reader of the visibility document is most likely to act on.
//
// It asserts the ROW, not the prose: the value in the table has to be the value in the header, and
// the header has to be named. What the row says about WHY that number is what it is cannot be
// checked mechanically and is not attempted.
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "cnahouse/visibility/ClipFrustum.hpp"
#include "cnahouse/visibility/ClipRect.hpp"
#include "cnahouse/visibility/CullDistance.hpp"
#include "cnahouse/visibility/DetailSets.hpp"
#include "cnahouse/visibility/ExteriorBvh.hpp"
#include "cnahouse/visibility/ExteriorCulling.hpp"
#include "cnahouse/visibility/PortalRuntime.hpp"
#include "cnahouse/visibility/PortalTraversal.hpp"

namespace
{
    namespace vis = cnahouse::visibility;

    constexpr const char* kDoc = "docs/visibility.md";

    std::string ReadDoc()
    {
        std::ifstream in(kDoc, std::ios::binary);
        EXPECT_TRUE(in.is_open()) << kDoc;
        std::string text(static_cast<std::size_t>(std::filesystem::file_size(kDoc)), '\0');
        in.read(text.data(), static_cast<std::streamsize>(text.size()));
        text.resize(static_cast<std::size_t>(in.gcount()));
        return text;
    }

    /// One row of §3's table: the first cell EXACTLY as the document spells it -- back quotes and
    /// all, because guessing at the spelling is how a check like this passes on the wrong row --
    /// the value it must carry, and the header it must name.
    struct Row
    {
        std::string cell;
        std::string value;
        std::string header;
    };

    /// `45 / 70 / 120 / 180 / 90 / 160 / 300 / 420 m`, from the array itself.
    std::string CullDistanceList()
    {
        std::string list;
        for (const float metres : vis::kCullDistances)
        {
            list += (list.empty() ? "" : " / ") + std::format("{:g}", static_cast<double>(metres));
        }
        return list + " m";
    }

} // namespace

TEST(VisibilityDocTests, TheParameterTableCarriesTheValuesTheHeadersHave)
{
    if (!std::filesystem::exists(kDoc))
    {
        GTEST_SKIP() << "no " << kDoc << "; this test runs from the repository root";
    }
    const std::string doc = ReadDoc();

    const std::vector<Row> rows{
        {"`kMaxFrustaPerCell`", std::to_string(vis::kMaxFrustaPerCell), "PortalTraversal.hpp"},
        {"`kMaxVisibleCells`", std::to_string(vis::kMaxVisibleCells), "PortalTraversal.hpp"},
        {"`kMaxQueuedCones`", std::to_string(vis::kMaxQueuedCones), "PortalTraversal.hpp"},
        {"`kMaxClippedVertices`", std::to_string(vis::kMaxClippedVertices), "ClipRect.hpp"},
        {"`ClipFrustum::kMaxPlanes`", std::to_string(vis::ClipFrustum::kMaxPlanes), "ClipFrustum.hpp"},
        // The one value the table spells in a form no format string produces. The literal below is
        // the document's own text, and this row is what ties the two together.
        {"`kMinPortalNdcArea`", "1.2e-5", "PortalArea.hpp"},
        {"`ExteriorCones::kMaxCones`", std::to_string(vis::ExteriorCones::kMaxCones), "ExteriorCulling.hpp"},
        {"BVH `kLevels`, `kBranching`, `kMinToSplit`",
         std::format("{}, {}, {}",
                     vis::ExteriorBvh::kLevels,
                     vis::ExteriorBvh::kBranching,
                     vis::ExteriorBvh::kMinToSplit),
         "ExteriorBvh.hpp"},
        {"cull distances", CullDistanceList(), "CullDistance.hpp"},
        {"`kClosedBelow`, `kOpenAbove`",
         std::format("{:g}, {:g}",
                     static_cast<double>(vis::PortalRuntime::kClosedBelow),
                     static_cast<double>(vis::PortalRuntime::kOpenAbove)),
         "PortalRuntime.hpp"},
        {"`kDressingDistance`, `kMicroDistance`",
         std::format("{:g} m, {:g} m",
                     static_cast<double>(vis::kDressingDistance),
                     static_cast<double>(vis::kMicroDistance)),
         "DetailSets.hpp"},
    };

    // §25.2's cutoff, tied to the document's own spelling of it: `1.2e-5` is what the table says
    // and this is the assertion that makes that the header's number rather than a coincidence.
    EXPECT_FLOAT_EQ(vis::kMinPortalNdcArea, 1.2e-5F);

    for (const Row& row : rows)
    {
        const std::size_t at = doc.find("| " + row.cell + " | ");
        ASSERT_NE(at, std::string::npos)
            << kDoc << " has no §3 row for " << row.cell
            << ". A constant with no row is a constant a reader of that document will not know about.";
        const std::size_t end = doc.find('\n', at);
        const std::string line = doc.substr(at, end - at);
        EXPECT_NE(line.find("| " + row.value + " |"), std::string::npos)
            << row.cell << " is " << row.value << " in the header and the document says otherwise:\n"
            << line;
        EXPECT_NE(line.find(row.header), std::string::npos)
            << row.cell << " must name the header that owns it (" << row.header << "):\n"
            << line;
    }
    std::printf("  %zu constant(s) checked against docs/visibility.md\n", rows.size());
}

TEST(VisibilityDocTests, TheDocumentNamesTheOverlaysAndTheSwitchesThatExist)
{
    // The debugging half is the half a person reads under pressure, and a key or a command that
    // has been renamed since it was written is worse than no document at all.
    if (!std::filesystem::exists(kDoc))
    {
        GTEST_SKIP() << "no " << kDoc;
    }
    const std::string doc = ReadDoc();
    for (const char* mention : {"`F3`", "`F4`", "`F5`", "cull off", "--no-cull"})
    {
        EXPECT_NE(doc.find(mention), std::string::npos) << mention << " is not mentioned in " << kDoc;
    }
    // And the files it points at are real. A document that names a source file that has moved
    // sends its reader to look for something that is not there.
    for (const char* path : {"include/cnahouse/visibility/PortalTraversal.hpp",
                             "include/cnahouse/visibility/ConeQueue.hpp",
                             "include/cnahouse/visibility/ExteriorCulling.hpp",
                             "src/visibility/PortalTraversal.cpp",
                             "src/debug/VisibilityOverlay.cpp",
                             "tests/perf/VisibilityCostTests.cpp"})
    {
        EXPECT_TRUE(std::filesystem::exists(path)) << path << ", named by " << kDoc;
    }
}
