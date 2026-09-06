// HOUSE-00099 / `BL-05` -- `VideoPlayer` with `CNA_ENABLE_VIDEO=OFF`: does `Play()` throw
// `NotSupportedException`, and does the rest of the program still LINK?
//
// The linking half is the half that actually matters to `cna-house`. Phase 21's television must be
// a feature that can be absent on a platform without the rest of the house failing to build, so
// this probe deliberately keeps referring to `Video`, `VideoPlayer` and `MediaState` after the
// failure — a probe that stopped at the exception would not have tested the claim it was written
// for.
//
// Built in `build-consumer/`, configured from the same source directory with
// `-DCNA_ENABLE_VIDEO=OFF`; see the note in `build-probe/CMakeLists.txt`.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Media/MediaState.hpp"
#include "Microsoft/Xna/Framework/Media/Video/Video.hpp"
#include "Microsoft/Xna/Framework/Media/Video/VideoPlayer.hpp"

#include <memory>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Media;

namespace
{

    class VideoOffProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            Content::ContentManager& content = getContentProperty();
            content.setRootDirectoryProperty("build-probe/p1-content");

            // The types must still be usable as types, or the "rest still links" claim is empty.
            VideoPlayer player;
            r.check("a VideoPlayer can still be constructed with video disabled", true);
            r.check("a fresh player still reports Stopped",
                    player.getStateProperty() == MediaState::Stopped,
                    std::to_string(static_cast<int>(player.getStateProperty())));

            // Loading is a separate question from playing, and the two can fail differently.
            std::unique_ptr<Video> video;
            std::string loadErr;
            try
            {
                video = std::make_unique<Video>(content.Load<Video>("P1Clip"));
            }
            catch (const std::exception& e)
            {
                loadErr = e.what();
            }
            r.note("Load<Video> with video disabled", video ? "succeeded" : ("threw: " + loadErr));

            bool threw = false;
            std::string what;
            try
            {
                player.Play(video ? video.get() : nullptr);
            }
            catch (const std::exception& e)
            {
                threw = true;
                what = e.what();
            }
            r.note("VideoPlayer::Play with video disabled", threw ? ("threw: " + what) : "did not throw");
            r.check("Play refuses rather than silently doing nothing",
                    threw,
                    threw ? what : "it returned normally, which would be worse than an exception");
            r.check("the refusal names the unsupported operation",
                    what.find("upport") != std::string::npos || what.find("ideo") != std::string::npos,
                    what);

            // The point of the whole probe: the program is still running and still calling into the
            // media API after the refusal.
            r.check("the player is still usable as an object after the refusal",
                    player.getStateProperty() == MediaState::Stopped,
                    "state after the failed Play: " +
                        std::to_string(static_cast<int>(player.getStateProperty())));
            player.Stop();
            r.check("Stop is still callable", true);

            return r.finish("p1-videooff");
        }
    };

} // namespace

P1_MAIN(VideoOffProbe, "p1-videooff")
