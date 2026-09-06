// HOUSE-00105 -- the `HEADLESS` renderer: does `Update` run with no window and no GPU?
//
// This is the foundation of every later CI test, so the acceptance criterion is taken literally:
// **600 frames with no display server**, `DISPLAY` unset. The probe asserts that `DISPLAY` really
// is unset from inside the process, because a test that silently ran against an X server would
// prove nothing about CI and would be the easiest possible thing to get wrong.
#include "p1-common.hpp"

#include <cstdlib>

using namespace Microsoft::Xna::Framework;

namespace
{

    constexpr int kFrames = 600;

    class HeadlessProbe : public Game
    {
    public:
        HeadlessProbe()
            : gdm_(this)
        {
            gdm_.setPreferredBackBufferWidthProperty(320);
            gdm_.setPreferredBackBufferHeightProperty(240);
            gdm_.setSynchronizeWithVerticalRetraceProperty(false);
            setIsFixedTimeStepProperty(false);
        }

        [[nodiscard]] int exitCode() const
        {
            return code_;
        }

    protected:
        void Initialize() override
        {
            Game::Initialize();
            initialized_ = true;
        }

        void Update(GameTime& gameTime) override
        {
            Game::Update(gameTime);
            ++updates_;
            elapsed_ += gameTime.getElapsedGameTimeProperty().getTotalSecondsProperty();
            if (updates_ >= kFrames)
            {
                Report();
                Exit();
            }
        }

        void Draw(const GameTime& gameTime) override
        {
            Game::Draw(gameTime);
            ++draws_;
        }

    private:
        void Report()
        {
            p1::Report r;
            const char* display = std::getenv("DISPLAY");
            const char* wayland = std::getenv("WAYLAND_DISPLAY");
            r.note("DISPLAY", display && *display ? display : "<unset>");
            r.note("WAYLAND_DISPLAY", wayland && *wayland ? wayland : "<unset>");
            // The criterion says "no display server". Asserting it from inside the process is what
            // makes the rest of the run mean something.
            r.check("no display server is reachable from this process",
                    (display == nullptr || *display == '\0') && (wayland == nullptr || *wayland == '\0'),
                    "a run against a live server would prove nothing about CI");
            r.check("Initialize ran", initialized_);
            r.check("Update ran for 600 frames", updates_ >= kFrames, std::to_string(updates_));
            r.note("Draw calls", std::to_string(draws_));
            r.check(
                "Draw was also called, so the frame loop is complete", draws_ > 0, std::to_string(draws_));
            char t[80];
            std::snprintf(t, sizeof t, "%.4f s of GameTime across %d updates", elapsed_, updates_);
            r.note("elapsed GameTime", t);
            r.check("GameTime advances", elapsed_ > 0.0, t);
            code_ = r.finish("p1-headless");
        }

        GraphicsDeviceManager gdm_;
        bool initialized_ = false;
        int updates_ = 0;
        int draws_ = 0;
        double elapsed_ = 0.0;
        int code_ = 3;
    };

} // namespace

P1_MAIN(HeadlessProbe, "p1-headless")
