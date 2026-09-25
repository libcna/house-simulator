// SPDX-License-Identifier: MIT
//
// HOUSE-02891 -- the deliberately small Emscripten proof before the full application build.
// One open-front room, one table-shaped prop and one preloaded SoundEffect exercise the WEBGL2,
// Asyncify, exception and packaged-content contracts without pulling the house runtime into the
// diagnosis. Game::Run owns the browser loop, exactly as it does in the shipping application.

#include <array>
#include <cstdio>
#include <exception>
#include <memory>
#include <optional>
#include <string>

#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundState.hpp"
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Game.hpp"
#include "Microsoft/Xna/Framework/GameTime.hpp"
#include "Microsoft/Xna/Framework/Graphics/BasicEffect.hpp"
#include "Microsoft/Xna/Framework/Graphics/DepthFormat.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPass.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectPassCollection.hpp"
#include "Microsoft/Xna/Framework/Graphics/EffectTechnique.hpp"
#include "Microsoft/Xna/Framework/Graphics/GraphicsDevice.hpp"
#include "Microsoft/Xna/Framework/Graphics/PrimitiveType.hpp"
#include "Microsoft/Xna/Framework/Graphics/RasterizerState.hpp"
#include "Microsoft/Xna/Framework/Graphics/VertexPositionColor.hpp"
#include "Microsoft/Xna/Framework/GraphicsDeviceManager.hpp"
#include "Microsoft/Xna/Framework/Matrix.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"

namespace cnahouse::app
{
    namespace
    {
        namespace Xna = Microsoft::Xna::Framework;
        namespace Graphics = Microsoft::Xna::Framework::Graphics;

        constexpr int kWidth = 800;
        constexpr int kHeight = 450;
        constexpr int kFramesToRun = 90;
        constexpr float kFieldOfView = 0.78539816339F;

        using Vertex = Graphics::VertexPositionColor;

        void PutQuad(Vertex* output,
                     const Xna::Vector3& a,
                     const Xna::Vector3& b,
                     const Xna::Vector3& c,
                     const Xna::Vector3& d,
                     const Xna::Color& colour)
        {
            output[0] = Vertex(a, colour);
            output[1] = Vertex(b, colour);
            output[2] = Vertex(c, colour);
            output[3] = Vertex(a, colour);
            output[4] = Vertex(c, colour);
            output[5] = Vertex(d, colour);
        }

        void PutBox(Vertex* output,
                    const Xna::Vector3& minimum,
                    const Xna::Vector3& maximum,
                    const Xna::Color& colour)
        {
            PutQuad(output + 0,
                    Xna::Vector3(minimum.X, minimum.Y, maximum.Z),
                    Xna::Vector3(maximum.X, minimum.Y, maximum.Z),
                    Xna::Vector3(maximum.X, maximum.Y, maximum.Z),
                    Xna::Vector3(minimum.X, maximum.Y, maximum.Z),
                    colour);
            PutQuad(output + 6,
                    Xna::Vector3(maximum.X, minimum.Y, minimum.Z),
                    Xna::Vector3(minimum.X, minimum.Y, minimum.Z),
                    Xna::Vector3(minimum.X, maximum.Y, minimum.Z),
                    Xna::Vector3(maximum.X, maximum.Y, minimum.Z),
                    colour);
            PutQuad(output + 12,
                    Xna::Vector3(minimum.X, minimum.Y, minimum.Z),
                    Xna::Vector3(minimum.X, minimum.Y, maximum.Z),
                    Xna::Vector3(minimum.X, maximum.Y, maximum.Z),
                    Xna::Vector3(minimum.X, maximum.Y, minimum.Z),
                    colour);
            PutQuad(output + 18,
                    Xna::Vector3(maximum.X, minimum.Y, maximum.Z),
                    Xna::Vector3(maximum.X, minimum.Y, minimum.Z),
                    Xna::Vector3(maximum.X, maximum.Y, minimum.Z),
                    Xna::Vector3(maximum.X, maximum.Y, maximum.Z),
                    colour);
            PutQuad(output + 24,
                    Xna::Vector3(minimum.X, maximum.Y, maximum.Z),
                    Xna::Vector3(maximum.X, maximum.Y, maximum.Z),
                    Xna::Vector3(maximum.X, maximum.Y, minimum.Z),
                    Xna::Vector3(minimum.X, maximum.Y, minimum.Z),
                    colour);
            PutQuad(output + 30,
                    Xna::Vector3(minimum.X, minimum.Y, minimum.Z),
                    Xna::Vector3(maximum.X, minimum.Y, minimum.Z),
                    Xna::Vector3(maximum.X, minimum.Y, maximum.Z),
                    Xna::Vector3(minimum.X, minimum.Y, maximum.Z),
                    colour);
        }

        std::array<Vertex, 60> MakeScene()
        {
            const Xna::Color floorColour(122, 91, 64, 255);
            const Xna::Color wallColour(196, 201, 207, 255);
            const Xna::Color propColour(42, 105, 128, 255);
            std::array<Vertex, 60> vertices{};

            // An open-front room: floor, rear wall and two side walls.
            PutQuad(vertices.data() + 0,
                    Xna::Vector3(-3.0F, 0.0F, -3.0F),
                    Xna::Vector3(3.0F, 0.0F, -3.0F),
                    Xna::Vector3(3.0F, 0.0F, 3.0F),
                    Xna::Vector3(-3.0F, 0.0F, 3.0F),
                    floorColour);
            PutQuad(vertices.data() + 6,
                    Xna::Vector3(3.0F, 0.0F, -3.0F),
                    Xna::Vector3(-3.0F, 0.0F, -3.0F),
                    Xna::Vector3(-3.0F, 3.0F, -3.0F),
                    Xna::Vector3(3.0F, 3.0F, -3.0F),
                    wallColour);
            PutQuad(vertices.data() + 12,
                    Xna::Vector3(-3.0F, 0.0F, -3.0F),
                    Xna::Vector3(-3.0F, 0.0F, 3.0F),
                    Xna::Vector3(-3.0F, 3.0F, 3.0F),
                    Xna::Vector3(-3.0F, 3.0F, -3.0F),
                    wallColour);
            PutQuad(vertices.data() + 18,
                    Xna::Vector3(3.0F, 0.0F, 3.0F),
                    Xna::Vector3(3.0F, 0.0F, -3.0F),
                    Xna::Vector3(3.0F, 3.0F, -3.0F),
                    Xna::Vector3(3.0F, 3.0F, 3.0F),
                    wallColour);

            // One unmistakable prop: a compact table/plinth in the room centre.
            PutBox(vertices.data() + 24,
                   Xna::Vector3(-0.85F, 0.0F, -0.75F),
                   Xna::Vector3(0.85F, 0.85F, 0.75F),
                   propColour);
            return vertices;
        }
    } // namespace

    class WebSpikeGame final : public Xna::Game
    {
    public:
        WebSpikeGame()
            : graphics_(this)
            , vertices_(MakeScene())
        {
            graphics_.setPreferredBackBufferWidthProperty(kWidth);
            graphics_.setPreferredBackBufferHeightProperty(kHeight);
            graphics_.setPreferredDepthStencilFormatProperty(Graphics::DepthFormat::Depth24);
            getContentProperty().setRootDirectoryProperty("content");
            getWindowProperty().setTitleProperty("House Simulator WebGL2 spike");
        }

        [[nodiscard]] bool Succeeded() const
        {
            return contentLoaded_ && soundStarted_ && drawnFrames_ > 0;
        }

        [[nodiscard]] int DrawnFrames() const
        {
            return drawnFrames_;
        }

    protected:
        void Initialize() override
        {
            Xna::Game::Initialize();
            auto& device = getGraphicsDeviceProperty();
            device.setRasterizerStateProperty(Graphics::RasterizerState::CullNone);
            effect_ = std::make_unique<Graphics::BasicEffect>(device);
            effect_->setVertexColorEnabledProperty(true);
            effect_->setLightingEnabledProperty(false);
        }

        void LoadContent() override
        {
            sound_.emplace(getContentProperty().Load<Xna::Audio::SoundEffect>("Audio/Smoke/chime"));
            contentLoaded_ = true;
            soundInstance_ = std::make_unique<Xna::Audio::SoundEffectInstance>(sound_->CreateInstance());
            soundInstance_->Play();
            soundStarted_ = soundInstance_->getStateProperty() == Xna::Audio::SoundState::Playing;
        }

        void Update(Xna::GameTime& gameTime) override
        {
            Xna::Game::Update(gameTime);
            ++updatedFrames_;
            if (updatedFrames_ >= kFramesToRun)
            {
                Exit();
            }
        }

        void Draw(const Xna::GameTime& gameTime) override
        {
            Xna::Game::Draw(gameTime);
            auto& device = getGraphicsDeviceProperty();
            device.Clear(Xna::Color(30, 39, 48, 255));
            device.setRasterizerStateProperty(Graphics::RasterizerState::CullNone);

            const auto& viewport = device.getViewportProperty();
            const float aspect = static_cast<float>(viewport.getWidthProperty()) /
                                 static_cast<float>(viewport.getHeightProperty());
            effect_->setWorldProperty(Xna::Matrix::getIdentityProperty());
            effect_->setViewProperty(Xna::Matrix::CreateLookAt(
                Xna::Vector3(0.0F, 1.7F, 6.8F), Xna::Vector3(0.0F, 1.1F, -0.5F), Xna::Vector3::Up));
            effect_->setProjectionProperty(
                Xna::Matrix::CreatePerspectiveFieldOfView(kFieldOfView, aspect, 0.1F, 50.0F));

            auto& passes = effect_->getCurrentTechniqueProperty()->getPassesProperty();
            for (int pass = 0; pass < passes.getCountProperty(); ++pass)
            {
                passes[pass]->Apply();
                device.DrawUserPrimitives(Graphics::PrimitiveType::TriangleList,
                                          vertices_.data(),
                                          0,
                                          static_cast<int>(vertices_.size() / 3));
            }
            ++drawnFrames_;
        }

    private:
        Xna::GraphicsDeviceManager graphics_;
        std::array<Vertex, 60> vertices_;
        std::unique_ptr<Graphics::BasicEffect> effect_;
        std::optional<Xna::Audio::SoundEffect> sound_;
        std::unique_ptr<Xna::Audio::SoundEffectInstance> soundInstance_;
        int updatedFrames_ = 0;
        int drawnFrames_ = 0;
        bool contentLoaded_ = false;
        bool soundStarted_ = false;
    };
} // namespace cnahouse::app

int main()
{
    try
    {
        cnahouse::app::WebSpikeGame game;
        game.Run();
        if (!game.Succeeded())
        {
            std::fprintf(
                stderr, "HOUSE-02891 WEB SPIKE FAIL room=1 prop=1 sound=0 frames=%d\n", game.DrawnFrames());
            return 1;
        }
        std::printf("HOUSE-02891 WEB SPIKE PASS room=1 prop=1 sound=1 frames=%d\n", game.DrawnFrames());
        std::fflush(stdout);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "HOUSE-02891 WEB SPIKE FAIL: %s\n", error.what());
        return 1;
    }
}
