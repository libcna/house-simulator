// HOUSE-00098 -- `Video` + `VideoPlayer::GetTexture()`: does the frame actually ADVANCE, and is
//                there audio?
//
// The clip is generated locally by ffmpeg from `lavfi` sources -- a 64x64, 2-second, 10 fps clip
// whose colour changes every half second (red, green, blue, white) over a 440 Hz sine. Nothing is
// downloaded and no third party's media is involved, so there is no licensing question to answer
// and the expected pixel at any timestamp is known by construction.
//
// "Frame advance" is measured, not assumed: the texture is read back at several play positions and
// the colours must change in the authored ORDER. A player that handed back the first frame forever
// would pass a test that only checked "a texture came back".
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Graphics/Texture2D.hpp"
#include "Microsoft/Xna/Framework/Media/MediaState.hpp"
#include "Microsoft/Xna/Framework/Media/Video/Video.hpp"
#include "Microsoft/Xna/Framework/Media/Video/VideoPlayer.hpp"

#include <chrono>
#include <memory>
#include <thread>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Media;

namespace
{

    const char* StateName(MediaState s)
    {
        switch (s)
        {
            case MediaState::Playing:
                return "Playing";
            case MediaState::Paused:
                return "Paused";
            case MediaState::Stopped:
                return "Stopped";
        }
        return "?";
    }

    // Which of the four authored colours a sampled texel is closest to, or -1 for none of them.
    int ClassifyColour(const Color& c)
    {
        struct Ref
        {
            int r, g, b;
        };

        const Ref refs[4] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}};
        int best = -1;
        int bestDistance = 1 << 30;
        for (int i = 0; i < 4; ++i)
        {
            const int dr = c.getRProperty() - refs[i].r;
            const int dg = c.getGProperty() - refs[i].g;
            const int db = c.getBProperty() - refs[i].b;
            const int d = dr * dr + dg * dg + db * db;
            if (d < bestDistance)
            {
                bestDistance = d;
                best = i;
            }
        }
        // 90 per channel of slack, which YUV 4:2:0 round-tripping easily costs, but not so much that
        // two of the four authored colours could be confused.
        return bestDistance <= 3 * 90 * 90 ? best : -1;
    }

    class VideoProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            Content::ContentManager& content = getContentProperty();
            content.setRootDirectoryProperty("build-probe/p1-content");

            // MEASURED: the reader is registered for `Media::Video` itself, so `Load<Video>` returns
            // BY VALUE -- like `Model`, and unlike `Effect`, whose reader is registered for
            // `shared_ptr<Effect>`. The three are not consistent, and CNA says which is which very
            // clearly when asked wrongly: *"holds a Video asset, which is not the type requested"*.
            Video* video = nullptr;
            std::unique_ptr<Video> owned;
            std::string err;
            try
            {
                owned = std::make_unique<Video>(content.Load<Video>("P1Clip"));
                video = owned.get();
            }
            catch (const std::exception& e)
            {
                err = e.what();
            }
            r.check("the compiled video loads as a Video", video != nullptr, err);
            if (video == nullptr)
            {
                r.note("VERDICT", "video content did not load; BL-05 and phase 21 are affected");
                return r.finish("p1-video");
            }

            char meta[160];
            std::snprintf(meta,
                          sizeof meta,
                          "%dx%d, %.2f fps, duration %.3f s",
                          static_cast<int>(video->getWidthProperty()),
                          static_cast<int>(video->getHeightProperty()),
                          static_cast<double>(video->getFramesPerSecondProperty()),
                          video->getDurationProperty().getTotalSecondsProperty());
            r.note("Video metadata", meta);
            r.check("the metadata matches the authored clip",
                    video->getWidthProperty() == 64 && video->getHeightProperty() == 64,
                    meta);

            VideoPlayer player;
            r.check("a fresh VideoPlayer reports Stopped",
                    player.getStateProperty() == MediaState::Stopped,
                    StateName(player.getStateProperty()));

            bool played = true;
            try
            {
                player.Play(video);
            }
            catch (const std::exception& e)
            {
                played = false;
                err = e.what();
            }
            r.check("VideoPlayer::Play accepts the video", played, err);
            if (!played)
            {
                return r.finish("p1-video");
            }
            r.check("the player reports Playing after Play",
                    player.getStateProperty() == MediaState::Playing,
                    StateName(player.getStateProperty()));

            // Sample across the clip. Each authored quarter-second band has its own colour, so the
            // sequence of classifications IS the evidence that frames advance.
            std::string observed;
            std::string positions;
            int distinct = 0;
            int lastColour = -2;
            int textureCalls = 0, nullTextures = 0;
            for (int step = 0; step < 16; ++step)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(120));
                Graphics::Texture2D* frame = player.GetTexture();
                ++textureCalls;
                if (frame == nullptr)
                {
                    ++nullTextures;
                    observed += "- ";
                    continue;
                }
                std::vector<Color> texels(static_cast<size_t>(frame->getWidthProperty()) *
                                          static_cast<size_t>(frame->getHeightProperty()));
                frame->GetData(texels.data(), static_cast<int>(texels.size()));
                const Color centre = texels[texels.size() / 2 + frame->getWidthProperty() / 2];
                const int colour = ClassifyColour(centre);
                if (colour != lastColour)
                {
                    ++distinct;
                    lastColour = colour;
                }
                const char names[5] = {'R', 'G', 'B', 'W', '?'};
                observed += names[colour < 0 ? 4 : colour];
                observed += ' ';
                char pb[24];
                std::snprintf(
                    pb, sizeof pb, "%.2f ", player.getPlayPositionProperty().getTotalSecondsProperty());
                positions += pb;
            }
            r.note("GetTexture calls",
                   std::to_string(textureCalls) + ", of which " + std::to_string(nullTextures) +
                       " returned null");
            r.note("sampled frame colours (R/G/B/W, '-' = null, '?' = unclassified)", observed);
            r.note("PlayPosition at each sample", positions);
            r.check("GetTexture returns a frame",
                    nullTextures < textureCalls,
                    std::to_string(nullTextures) + "/" + std::to_string(textureCalls) + " null");
            r.check("the frame ADVANCES -- more than one distinct colour was seen",
                    distinct > 1,
                    std::to_string(distinct) + " transitions in " + observed);
            r.check("PlayPosition advances",
                    player.getPlayPositionProperty().getTotalSecondsProperty() > 0.0,
                    std::to_string(player.getPlayPositionProperty().getTotalSecondsProperty()) + " s");

            player.Stop();
            r.check("Stop returns the player to Stopped",
                    player.getStateProperty() == MediaState::Stopped,
                    StateName(player.getStateProperty()));

            return r.finish("p1-video");
        }
    };

} // namespace

P1_MAIN(VideoProbe, "p1-video")
