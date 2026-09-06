// SPDX-License-Identifier: MIT
#pragma once

#include <functional>
#include <string>

#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"

namespace cnahouse::testsupport
{

    /// @brief Runs @p body once inside a real frame, with a live `GraphicsDevice`, then exits.
    ///
    /// **Why a real device and not a mock.** The types this helper exists for -- `StateTracker`,
    /// `Renderer` -- exist in order to talk to a device. A test that substituted a mock would verify
    /// only that the mock and the code under test agree with each other, which is the one thing that
    /// is never in doubt. Under the `headless` preset this costs about 0.6 s per test and gets a real
    /// device with no window, so a state the device rejects fails here rather than in a nightly
    /// render job.
    class DeviceHost final : public Microsoft::Xna::Framework::Game
    {
    public:
        using Body = std::function<void(Microsoft::Xna::Framework::Graphics::GraphicsDevice&)>;

        explicit DeviceHost(Body body)
            : gdm_(this)
            , body_(std::move(body))
        {
            gdm_.setPreferredBackBufferWidthProperty(320);
            gdm_.setPreferredBackBufferHeightProperty(240);
            gdm_.setSynchronizeWithVerticalRetraceProperty(false);
            setIsFixedTimeStepProperty(false);
        }

        [[nodiscard]] bool Ran() const noexcept
        {
            return ran_;
        }

        [[nodiscard]] const std::string& Failure() const noexcept
        {
            return failure_;
        }

    protected:
        void Draw(const Microsoft::Xna::Framework::GameTime& gameTime) override
        {
            Game::Draw(gameTime);
            if (ran_)
            {
                return;
            }
            ran_ = true;
            try
            {
                body_(getGraphicsDeviceProperty());
            }
            catch (const std::exception& e)
            {
                // Recorded rather than rethrown: an exception escaping `Draw` would unwind through
                // XNA's frame loop, and what the test wants to report is the message, not a crash.
                failure_ = e.what();
            }
            Exit();
        }

    private:
        Microsoft::Xna::Framework::GraphicsDeviceManager gdm_;
        Body body_;
        bool ran_ = false;
        std::string failure_;
    };

} // namespace cnahouse::testsupport
