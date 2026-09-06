// HOUSE-00100 -- `Mouse::GetState` + `SetPosition` recentring: delta accuracy and drift over
//                10 000 frames. Not shortened: the whole point of the acceptance criterion is that
//                a per-frame error of a fraction of a pixel is invisible in 100 frames and ruins a
//                first-person camera in 10 000.
// HOUSE-00101 -- `Keyboard`, `GamePad`, and the presence/behaviour of `TouchPanel` on desktop.
//
// This probe does its work in `Update` across real frames rather than in one `Draw`, because
// recentring is a per-frame interaction with the window system and cannot be simulated in a loop
// inside a single frame.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Input/GamePad.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadCapabilities.hpp"
#include "Microsoft/Xna/Framework/Input/GamePadState.hpp"
#include "Microsoft/Xna/Framework/Input/Keyboard.hpp"
#include "Microsoft/Xna/Framework/Input/KeyboardState.hpp"
#include "Microsoft/Xna/Framework/Input/Keys.hpp"
#include "Microsoft/Xna/Framework/Input/Mouse.hpp"
#include "Microsoft/Xna/Framework/Input/MouseState.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchCollection.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchPanel.hpp"
#include "Microsoft/Xna/Framework/Input/Touch/TouchPanelCapabilities.hpp"
#include "Microsoft/Xna/Framework/PlayerIndex.hpp"

#include <cstdlib>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Input;

namespace
{

    constexpr int kFrames = 10000;
    constexpr int kWidth = 800;
    constexpr int kHeight = 600;

    class InputProbe : public Game
    {
    public:
        InputProbe()
            : gdm_(this)
        {
            gdm_.setPreferredBackBufferWidthProperty(kWidth);
            gdm_.setPreferredBackBufferHeightProperty(kHeight);
            gdm_.setSynchronizeWithVerticalRetraceProperty(false);
            setIsFixedTimeStepProperty(false);
        }

        [[nodiscard]] int exitCode() const
        {
            return code_;
        }

    protected:
        void Update(GameTime& gameTime) override
        {
            Game::Update(gameTime);
            const int cx = kWidth / 2;
            const int cy = kHeight / 2;

            if (frame_ == 0)
            {
                Mouse::SetPosition(cx, cy);
                ++frame_;
                return;
            }

            // The recentring loop a first-person camera runs: read, take the delta from the centre,
            // put the cursor back. With no hand on the mouse every delta must be exactly zero, and any
            // systematic non-zero delta is drift that would slowly rotate the camera on its own.
            // `Game::IsActive` is plain XNA and is the gate a first-person camera must use. Counted
            // per frame so the recentring result can be attributed rather than guessed at.
            if (getIsActiveProperty())
            {
                ++activeFrames_;
            }

            const MouseState state = Mouse::GetState();
            const int dx = state.getXProperty() - cx;
            const int dy = state.getYProperty() - cy;
            // Counted only on frames where the window was ALSO focused on the previous frame, so the
            // warp issued last frame had a focused window to land in. The frame a window gains focus is
            // a transition, not a sample. `(0,0)` is counted separately: it is the position CNA reports
            // when the pointer is not over the window, and it is indistinguishable from a genuine
            // top-left corner reading only in theory -- the centre is (400,300), so a real delta of
            // exactly (-400,-300) every time is the sentinel, not the mouse.
            const bool active = getIsActiveProperty();
            if (state.getXProperty() == 0 && state.getYProperty() == 0)
            {
                ++sentinelFrames_;
            }
            if (active && wasActive_ && !(state.getXProperty() == 0 && state.getYProperty() == 0))
            {
                ++settledFrames_;
                if (dx != 0 || dy != 0)
                {
                    ++activeNonZero_;
                    activeSumDx_ += dx;
                    activeSumDy_ += dy;
                    if (std::abs(dx) > std::abs(activeWorstDx_))
                    {
                        activeWorstDx_ = dx;
                    }
                    if (std::abs(dy) > std::abs(activeWorstDy_))
                    {
                        activeWorstDy_ = dy;
                    }
                }
            }
            wasActive_ = active;
            if (dx != 0 || dy != 0)
            {
                ++nonZeroFrames_;
                sumDx_ += dx;
                sumDy_ += dy;
                if (std::abs(dx) > std::abs(worstDx_))
                {
                    worstDx_ = dx;
                }
                if (std::abs(dy) > std::abs(worstDy_))
                {
                    worstDy_ = dy;
                }
                if (firstNonZeroFrame_ < 0)
                {
                    firstNonZeroFrame_ = frame_;
                }
            }
            Mouse::SetPosition(cx, cy);

            // A single frame where the position is read straight back, to separate "SetPosition does
            // not take effect" from "something else moves the cursor".
            if (frame_ == 1)
            {
                const MouseState immediate = Mouse::GetState();
                immediateX_ = immediate.getXProperty();
                immediateY_ = immediate.getYProperty();
            }

            if (++frame_ >= kFrames)
            {
                Report();
                Exit();
            }
        }

        void Draw(const GameTime& gameTime) override
        {
            Game::Draw(gameTime);
        }

    private:
        void Report()
        {
            p1::Report r;
            char msg[240];
            std::snprintf(msg,
                          sizeof msg,
                          "%d of %d frames reported a non-zero delta; summed drift (%d, %d); "
                          "worst single frame (%d, %d); first at frame %d",
                          nonZeroFrames_,
                          kFrames - 1,
                          sumDx_,
                          sumDy_,
                          worstDx_,
                          worstDy_,
                          firstNonZeroFrame_);
            r.note("HOUSE-00100 recentring over 10 000 frames", msg);
            char am[320];
            std::snprintf(am,
                          sizeof am,
                          "%d of %d frames had IsActive true; %d frames read the unfocused (0,0) "
                          "sentinel; %d frames were settled (focused this frame AND last, and not the "
                          "sentinel); of those %d had a non-zero delta, summed (%d, %d), worst (%d, %d)",
                          activeFrames_,
                          kFrames - 1,
                          sentinelFrames_,
                          settledFrames_,
                          activeNonZero_,
                          activeSumDx_,
                          activeSumDy_,
                          activeWorstDx_,
                          activeWorstDy_);
            r.note("HOUSE-00100 with focus and the sentinel separated", am);
            char im[120];
            std::snprintf(im,
                          sizeof im,
                          "read back (%d, %d) immediately after SetPosition(%d, %d)",
                          immediateX_,
                          immediateY_,
                          kWidth / 2,
                          kHeight / 2);
            r.note("SetPosition -> GetState in the same frame", im);
            r.check("SetPosition is reflected by the very next GetState",
                    immediateX_ == kWidth / 2 && immediateY_ == kHeight / 2,
                    im);
            // The criterion is about the loop a FOCUSED game runs. An unfocused window's mouse state
            // is not input at all, and requiring zero drift from it would be requiring the wrong thing
            // -- so the check is stated against focused frames, and the unfocused behaviour is reported
            // as the finding it is.
            // What the 10 000 frames actually established, and what they could not.
            //
            // `Mouse::GetState` reads a SNAPSHOT the platform layer maintains from SDL mouse-motion
            // events (`modules/input/src/Xna/Mouse.cpp:117`). On an unattended desktop the pointer
            // never enters or moves over the probe window, no motion event ever arrives, and the
            // snapshot stays at its initial (0,0) for all 10 000 frames -- which is exactly what was
            // measured. The drift criterion needs real pointer motion over a focused window, and this
            // environment cannot supply it, so it is reported as INCONCLUSIVE rather than passed or
            // failed. Manufacturing a pass here would be the one thing phase 1 exists to prevent.
            const bool noMotionAtAll = sentinelFrames_ == kFrames - 1;
            r.check("the sample is explainable: either every frame was the no-motion snapshot, "
                    "or there were settled frames to judge",
                    noMotionAtAll || settledFrames_ > 1000,
                    am);
            if (noMotionAtAll)
            {
                r.note("HOUSE-00100 VERDICT",
                       "INCONCLUSIVE for the drift criterion. Every one of the 10 000 frames read the "
                       "no-motion snapshot (0,0), so no delta measured here is a delta. The task stays "
                       "OPEN in plan.md with this evidence; it needs a session with the pointer "
                       "actually over the window.");
            }
            else
            {
                r.check("the recentring loop accumulates NO drift across settled frames",
                        activeNonZero_ == 0,
                        am);
            }
            // These two ARE established regardless, and both change how the camera is written.
            r.check("Game::IsActive is NOT a proxy for 'the mouse is usable'",
                    activeFrames_ > 0 && sentinelFrames_ > 0,
                    "IsActive was true on " + std::to_string(activeFrames_) +
                        " frames while the mouse snapshot never advanced past (0,0) on " +
                        std::to_string(sentinelFrames_) + " of them");
            r.check("GetState reports an event-driven snapshot, not the live OS cursor",
                    sentinelFrames_ > 0,
                    "with no motion event the snapshot stays where it started, so a camera must seed "
                    "its previous position from a real motion event and never assume a centred start");

            // ================= HOUSE-00101 =========================================================
            const KeyboardState keys = Keyboard::GetState();
            const std::vector<Keys> pressed = keys.GetPressedKeys();
            r.note("Keyboard::GetState", std::to_string(pressed.size()) + " keys down");
            r.check("Keyboard::GetState answers, and reports a key that is not held as up",
                    !keys.IsKeyDown(Keys::F13),
                    "F13 is not on this keyboard and must read as up");
            r.check("IsKeyDown and GetPressedKeys agree",
                    [&]
                    {
                        for (const Keys k : pressed)
                        {
                            if (!keys.IsKeyDown(k))
                            {
                                return false;
                            }
                        }
                        return true;
                    }());

            for (int p = 0; p < 4; ++p)
            {
                const PlayerIndex index = static_cast<PlayerIndex>(p);
                const GamePadCapabilities caps = GamePad::GetCapabilities(index);
                const GamePadState state = GamePad::GetState(index);
                r.note(("GamePad player " + std::to_string(p + 1)).c_str(),
                       std::string("capabilities.IsConnected=") +
                           (caps.getIsConnectedProperty() ? "true" : "false") +
                           ", state.IsConnected=" + (state.getIsConnectedProperty() ? "true" : "false"));
                r.check(("GamePad player " + std::to_string(p + 1) +
                         ": capabilities and state agree about connection")
                            .c_str(),
                        caps.getIsConnectedProperty() == state.getIsConnectedProperty());
            }

            // TouchPanel on a desktop: the question is whether it is safely absent, not whether it
            // works. A game that queries it every frame must not pay for it or crash.
            bool touchOk = true;
            std::string touchErr;
            int touchCount = -1;
            bool touchConnected = false;
            try
            {
                const Touch::TouchPanelCapabilities caps = Touch::TouchPanel::GetCapabilities();
                touchConnected = caps.getIsConnectedProperty();
                const Touch::TouchCollection touches = Touch::TouchPanel::GetState();
                touchCount = touches.getCountProperty();
            }
            catch (const std::exception& e)
            {
                touchOk = false;
                touchErr = e.what();
            }
            r.check("TouchPanel can be queried on desktop without throwing", touchOk, touchErr);
            r.note("TouchPanel on desktop",
                   std::string("IsConnected=") + (touchConnected ? "true" : "false") +
                       ", GetState returned " + std::to_string(touchCount) + " touches");
            r.check("a desktop TouchPanel reports no touches", touchCount == 0, std::to_string(touchCount));

            code_ = r.finish("p1-input");
        }

        GraphicsDeviceManager gdm_;
        int frame_ = 0;
        int nonZeroFrames_ = 0;
        int sumDx_ = 0, sumDy_ = 0;
        int worstDx_ = 0, worstDy_ = 0;
        int firstNonZeroFrame_ = -1;
        int immediateX_ = -1, immediateY_ = -1;
        int activeFrames_ = 0;
        int activeNonZero_ = 0;
        int activeSumDx_ = 0, activeSumDy_ = 0;
        int activeWorstDx_ = 0, activeWorstDy_ = 0;
        int sentinelFrames_ = 0;
        int settledFrames_ = 0;
        bool wasActive_ = false;
        int code_ = 3;
    };

} // namespace

P1_MAIN(InputProbe, "p1-input")
