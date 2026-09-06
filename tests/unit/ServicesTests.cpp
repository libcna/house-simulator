// SPDX-License-Identifier: MIT
//
// `HOUSE-00128`'s acceptance: (1) construction order is explicit and asserted; (2) a system cannot
// resolve one constructed after it.
#include <gtest/gtest.h>

#include "cnahouse/app/Services.hpp"

namespace
{
    using cnahouse::app::Services;

    /// Counts construction and destruction so the ORDER of both can be asserted, which is the thing
    /// that actually goes wrong and the thing a plain struct of members gives no way to check.
    struct Trace
    {
        static std::vector<std::string>& Log()
        {
            static std::vector<std::string> log;
            return log;
        }
    };

    template<int N>
    struct Service
    {
        Service()
        {
            Trace::Log().push_back("construct " + std::to_string(N));
        }

        ~Service()
        {
            Trace::Log().push_back("destroy " + std::to_string(N));
        }

        int value = N;
    };

    class ServicesTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            Trace::Log().clear();
        }
    };

    TEST_F(ServicesTest, ConstructionOrderIsTheOrderOfRegistration)
    {
        Services services;
        services.Add<Service<1>>();
        services.Add<Service<2>>();
        services.Add<Service<3>>();

        ASSERT_EQ(Trace::Log().size(), 3u);
        EXPECT_EQ(Trace::Log()[0], "construct 1");
        EXPECT_EQ(Trace::Log()[1], "construct 2");
        EXPECT_EQ(Trace::Log()[2], "construct 3");
        EXPECT_EQ(services.Count(), 3u);
    }

    TEST_F(ServicesTest, DestructionIsTheExactReverse)
    {
        {
            Services services;
            services.Add<Service<1>>();
            services.Add<Service<2>>();
            services.Add<Service<3>>();
            Trace::Log().clear();
        }
        // A service destroyed before something holding a pointer to it is a use-after-free at
        // shutdown, which is the hardest kind to reproduce: it happens after the last thing anyone was
        // watching.
        ASSERT_EQ(Trace::Log().size(), 3u);
        EXPECT_EQ(Trace::Log()[0], "destroy 3");
        EXPECT_EQ(Trace::Log()[1], "destroy 2");
        EXPECT_EQ(Trace::Log()[2], "destroy 1");
    }

    TEST_F(ServicesTest, ResolveReturnsTheRegisteredInstance)
    {
        Services services;
        Service<1>& added = services.Add<Service<1>>();
        auto resolved = services.Resolve<Service<1>>();
        ASSERT_TRUE(resolved);
        EXPECT_EQ(*resolved, &added) << "the same object, not a copy";
    }

    TEST_F(ServicesTest, ResolvingSomethingUnregisteredNamesWhatWasAskedFor)
    {
        Services services;
        auto resolved = services.Resolve<Service<9>>();
        ASSERT_FALSE(resolved);
        EXPECT_EQ(resolved.Error().Code(), cnahouse::util::ErrorCode::NotFound);
        EXPECT_NE(resolved.Error().Message().find("no service of type"), std::string::npos);
    }

    TEST_F(ServicesTest, ASystemCannotResolveOneConstructedAfterIt)
    {
        // The second acceptance point, and the bug the container exists to make impossible: a service
        // holding a pointer to something built later is a null dereference at startup that reorders
        // itself every time someone adds a field.
        Services services;
        services.Add<Service<1>>();
        services.Add<Service<2>>();

        const std::size_t first = services.IndexOfService<Service<1>>();
        const std::size_t second = services.IndexOfService<Service<2>>();
        ASSERT_LT(first, second);

        // Later resolving earlier: fine.
        EXPECT_TRUE(services.ResolveFrom<Service<1>>(second));

        // Earlier resolving later: refused, with the positions named so the fix is obvious.
        auto backwards = services.ResolveFrom<Service<2>>(first);
        ASSERT_FALSE(backwards);
        EXPECT_EQ(backwards.Error().Code(), cnahouse::util::ErrorCode::InvalidData);
        EXPECT_NE(backwards.Error().Message().find("move it earlier in Bootstrap"), std::string::npos)
            << backwards.Error().Message();
    }

    TEST_F(ServicesTest, AServiceCanResolveItself)
    {
        // Not a pathological case: a system that looks itself up through the container during wiring
        // is resolving at its own index, which is not "after" and must be allowed.
        Services services;
        services.Add<Service<1>>();
        const std::size_t index = services.IndexOfService<Service<1>>();
        EXPECT_TRUE(services.ResolveFrom<Service<1>>(index));
    }

    TEST_F(ServicesTest, ABorrowedServiceIsResolvableButNotDestroyed)
    {
        // `GraphicsDevice` and `ContentManager` are owned by XNA. They must be resolvable and must not
        // be deleted by this container.
        Service<7> external;
        Trace::Log().clear();
        {
            Services services;
            services.AddBorrowed(external);
            auto resolved = services.Resolve<Service<7>>();
            ASSERT_TRUE(resolved);
            EXPECT_EQ((*resolved)->value, 7);
        }
        EXPECT_TRUE(Trace::Log().empty()) << "the container must not have destroyed a borrowed service";
    }

    TEST_F(ServicesTest, RegisteringTwiceIsRefusedRatherThanSilentlyReplacing)
    {
        // Replacing would leave the first instance leaked and every pointer to it dangling.
        Services services;
        Service<1>& first = services.Add<Service<1>>();
        services.Add<Service<1>>();
        auto resolved = services.Resolve<Service<1>>();
        ASSERT_TRUE(resolved);
        EXPECT_EQ(*resolved, &first) << "the first registration wins";
        EXPECT_EQ(services.Count(), 1u);
    }

    TEST_F(ServicesTest, ConstructionOrderIsReportableForDiagnostics)
    {
        Services services;
        services.Add<Service<1>>();
        services.Add<Service<2>>();
        const auto order = services.ConstructionOrder();
        ASSERT_EQ(order.size(), 2u);
        EXPECT_NE(order[0], order[1]);
    }

} // namespace
