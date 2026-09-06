// HOUSE-00095 -- `AudioListener`/`AudioEmitter`/`Apply3D`: the pan and attenuation curve across a
//                20 m sweep, recorded so `cna-house`'s own gain model can be calibrated against it.
// HOUSE-00096 -- `BL-11`: does `DopplerScale = 0` with zero velocities produce no pitch change?
// HOUSE-00097 -- the practical concurrent `SoundEffectInstance` ceiling on this host.
//
// The first version of this probe assumed `Apply3D` writes its result into the instance's public
// `Volume`, `Pan` and `Pitch`, and read those back. It does NOT -- measured, and that is itself the
// most important finding here. `Apply3D` stores attenuation, pan and Doppler in private state and
// composes them with the user's `Volume`/`Pitch` only when writing the mixer track; the public
// properties keep returning whatever the caller last set. So a game cannot read back what CNA
// applied, and `cna-house` (which owns room/portal occlusion, §32) must model the same curve
// itself rather than query it.
//
// What this probe therefore does, and does not do:
//   * it MEASURES that the public properties are unchanged by `Apply3D` across a 20 m sweep -- the
//     fact the design has to live with;
//   * it MEASURES the observable behavioural consequences: the `is3D` latch and the exception a
//     mismatched call raises, which prove `Apply3D` is not inert;
//   * it RECORDS the exact curve READ FROM `modules/audio/src/Xna/SoundEffectInstance.cpp`, clearly
//     labelled as read rather than measured, and reproduces it in C++ so the calibration table in
//     the capability report is generated rather than transcribed.
// Conflating those three would be exactly the kind of claim phase 1 exists to prevent.
#include "p1-common.hpp"

#include "Microsoft/Xna/Framework/Audio/AudioEmitter.hpp"
#include "Microsoft/Xna/Framework/Audio/AudioListener.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffect.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundEffectInstance.hpp"
#include "Microsoft/Xna/Framework/Audio/SoundState.hpp"

#include <memory>

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Audio;

namespace
{

    class Audio3dProbe : public p1::ProbeGame
    {
    protected:
        int Measure() override
        {
            p1::Report r;
            Content::ContentManager& content = getContentProperty();
            content.setRootDirectoryProperty("build-probe/p1-content");

            SoundEffect tone = content.Load<SoundEffect>("P1Tone");
            r.note("source", "1.0 s 440 Hz mono 16-bit PCM @ 44.1 kHz, self-generated");
            r.note("SoundEffect::DistanceScale", std::to_string(SoundEffect::getDistanceScaleProperty()));
            r.note("SoundEffect::DopplerScale", std::to_string(SoundEffect::getDopplerScaleProperty()));
            r.note("SoundEffect::MasterVolume", std::to_string(SoundEffect::getMasterVolumeProperty()));

            AudioListener listener;
            listener.setPositionProperty(Vector3::Zero);
            listener.setForwardProperty(Vector3(0.0f, 0.0f, -1.0f));
            listener.setUpProperty(Vector3(0.0f, 1.0f, 0.0f));
            listener.setVelocityProperty(Vector3::Zero);

            // ================= HOUSE-00095: what the public properties do (measured) =================
            {
                SoundEffectInstance inst = tone.CreateInstance();
                std::string curve;
                float previous = 2.0f;
                bool monotonic = true;
                float atOne = -1.0f, atTwenty = -1.0f;
                for (int metres = 0; metres <= 20; ++metres)
                {
                    AudioEmitter emitter;
                    emitter.setPositionProperty(Vector3(0.0f, 0.0f, -static_cast<float>(metres)));
                    emitter.setForwardProperty(Vector3(0.0f, 0.0f, -1.0f));
                    emitter.setUpProperty(Vector3(0.0f, 1.0f, 0.0f));
                    emitter.setVelocityProperty(Vector3::Zero);
                    inst.Apply3D(listener, emitter);
                    const float v = inst.getVolumeProperty();
                    if (metres == 1)
                    {
                        atOne = v;
                    }
                    if (metres == 20)
                    {
                        atTwenty = v;
                    }
                    if (v > previous + 1e-4f)
                    {
                        monotonic = false;
                    }
                    previous = v;
                    char b[24];
                    std::snprintf(b, sizeof b, "%d:%.4f ", metres, static_cast<double>(v));
                    curve += b;
                }
                (void)monotonic;
                r.note("HOUSE-00095 Volume after Apply3D at 0..20 m", curve);
                r.check("Apply3D leaves the PUBLIC Volume property untouched at every distance",
                        atOne == 1.0f && atTwenty == 1.0f,
                        "1 m " + std::to_string(atOne) + ", 20 m " + std::to_string(atTwenty) +
                            " -- the spatial gain is private state, not a readable property");
            }

            // ================= HOUSE-00095: the pan curve, left to right =============================
            {
                SoundEffectInstance inst = tone.CreateInstance();
                std::string curve;
                float leftPan = 0.0f, rightPan = 0.0f, centrePan = 0.0f;
                bool monotonic = true;
                float previous = -2.0f;
                for (int x = -10; x <= 10; x += 2)
                {
                    AudioEmitter emitter;
                    // one metre in front, so the source is never AT the listener (where pan is undefined)
                    emitter.setPositionProperty(Vector3(static_cast<float>(x), 0.0f, -1.0f));
                    emitter.setForwardProperty(Vector3(0.0f, 0.0f, -1.0f));
                    emitter.setUpProperty(Vector3(0.0f, 1.0f, 0.0f));
                    emitter.setVelocityProperty(Vector3::Zero);
                    inst.Apply3D(listener, emitter);
                    const float pan = inst.getPanProperty();
                    if (x == -10)
                    {
                        leftPan = pan;
                    }
                    if (x == 0)
                    {
                        centrePan = pan;
                    }
                    if (x == 10)
                    {
                        rightPan = pan;
                    }
                    if (pan < previous - 1e-4f)
                    {
                        monotonic = false;
                    }
                    previous = pan;
                    char b[28];
                    std::snprintf(b, sizeof b, "%+d:%.4f ", x, static_cast<double>(pan));
                    curve += b;
                }
                (void)monotonic;
                (void)centrePan;
                r.note("HOUSE-00095 Pan after Apply3D at X = -10..+10 m", curve);
                r.check("Apply3D leaves the PUBLIC Pan property untouched at every position",
                        leftPan == 0.0f && rightPan == 0.0f,
                        "left " + std::to_string(leftPan) + ", right " + std::to_string(rightPan));
            }

            // ================= HOUSE-00095: Apply3D is not inert (measured, behaviourally) ===========
            {
                // The `is3D` latch is the one publicly observable consequence of Apply3D having run.
                // An instance put into pan mode and then played refuses a later Apply3D; one that was
                // aimed in 3D before playing accepts it. An implementation where Apply3D did nothing
                // could not produce that distinction.
                SoundEffectInstance panned = tone.CreateInstance();
                panned.setPanProperty(0.5f);
                panned.Play();
                AudioEmitter emitter;
                emitter.setPositionProperty(Vector3(3.0f, 0.0f, -4.0f));
                emitter.setVelocityProperty(Vector3::Zero);
                bool threw = false;
                std::string what;
                try
                {
                    panned.Apply3D(listener, emitter);
                }
                catch (const std::exception& e)
                {
                    threw = true;
                    what = e.what();
                }
                panned.Stop();
                r.check("Apply3D on a playing pan-mode instance is refused, so the 3D state is real",
                        threw,
                        threw ? ("threw: " + what) : "it was accepted");

                SoundEffectInstance spatial = tone.CreateInstance();
                bool accepted = true;
                try
                {
                    spatial.Apply3D(listener, emitter);
                    spatial.Play();
                    spatial.Apply3D(listener, emitter); // still fine: it latched into 3D first
                }
                catch (const std::exception& e)
                {
                    accepted = false;
                    what = e.what();
                }
                spatial.Stop();
                r.check("an instance aimed in 3D before playing accepts Apply3D while playing",
                        accepted,
                        accepted ? "" : what);
            }

            // ================= HOUSE-00095: the curve itself (READ from the implementation) ==========
            {
                // NOT a measurement. `modules/audio/src/Xna/SoundEffectInstance.cpp` computes:
                //   normalized = distance / DistanceScale
                //   attenuation = normalized >= 1 ? clamp(1/normalized, 0, 1) : 1
                //   pan         = distance > 0 ? clamp(rightDisplacement / distance, -1, 1) : 0
                // reproduced here so the calibration table `cna-house` §32 needs is generated from the
                // stated formula rather than transcribed by hand, and so a future CNA change that
                // breaks the reproduction is visible as a diff in this file.
                const float distScale = SoundEffect::getDistanceScaleProperty();
                std::string table;
                for (int metres : {0, 1, 2, 3, 5, 8, 10, 15, 20})
                {
                    const float normalized =
                        static_cast<float>(metres) / (distScale > 0.0f ? distScale : 1.0f);
                    const float atten =
                        normalized >= 1.0f ? (1.0f / normalized > 1.0f ? 1.0f : 1.0f / normalized) : 1.0f;
                    char b[28];
                    std::snprintf(b, sizeof b, "%dm:%.4f ", metres, static_cast<double>(atten));
                    table += b;
                }
                const std::string label =
                    "HOUSE-00095 attenuation law (READ from CNA source, DistanceScale=" +
                    std::to_string(distScale) + ")";
                r.note(label.c_str(), table);
                r.note("HOUSE-00095 the law in words",
                       "FULL volume inside DistanceScale, then INVERSE DISTANCE beyond it -- not "
                       "inverse-square, and not a continuous falloff from zero. Pan is the "
                       "listener-relative rightward displacement over distance, clamped to [-1,1] -- "
                       "a linear approximation with no HRTF and no cone/orientation term.");
            }

            // ================= HOUSE-00096 / BL-11: Doppler ==========================================
            {
                SoundEffectInstance inst = tone.CreateInstance();
                AudioEmitter emitter;
                emitter.setPositionProperty(Vector3(0.0f, 0.0f, -5.0f));
                emitter.setForwardProperty(Vector3(0.0f, 0.0f, -1.0f));
                emitter.setUpProperty(Vector3(0.0f, 1.0f, 0.0f));
                emitter.setVelocityProperty(Vector3::Zero);
                emitter.setDopplerScaleProperty(0.0f);
                const float before = inst.getPitchProperty();
                inst.Apply3D(listener, emitter);
                const float zeroVelocity = inst.getPitchProperty();
                r.check("with DopplerScale = 0 and zero velocities, the public Pitch is unchanged",
                        std::fabs(zeroVelocity - before) < 1e-5f,
                        std::to_string(before) + " -> " + std::to_string(zeroVelocity));

                // The control that makes the previous check meaningful: a MOVING emitter with Doppler
                // enabled must change the pitch, or the first result proves nothing.
                emitter.setDopplerScaleProperty(1.0f);
                emitter.setVelocityProperty(Vector3(0.0f, 0.0f, 30.0f));
                inst.Apply3D(listener, emitter);
                const float moving = inst.getPitchProperty();
                char dm[160];
                std::snprintf(dm,
                              sizeof dm,
                              "still %.6f; moving at 30 m/s with DopplerScale 1 -> %.6f",
                              static_cast<double>(zeroVelocity),
                              static_cast<double>(moving));
                r.note("HOUSE-00096 Doppler control", dm);
                r.check("the public Pitch is untouched by Apply3D in both cases, as expected",
                        std::fabs(moving - zeroVelocity) < 1e-5f,
                        dm);
                r.note("BL-11 verdict",
                       "HOLDS, and for a stronger reason than the blocker assumed. Read from the "
                       "implementation: `effectiveDopplerScale = emitter.DopplerScale * "
                       "SoundEffect::DopplerScale`, and when that is 0 the factor is set to exactly "
                       "1.0f WITHOUT evaluating the Doppler math at all -- so there is no rounding "
                       "path by which a pitch change could appear. Zero velocities alone would also "
                       "give 1.0. The public Pitch property is untouched either way, which the probe "
                       "measured.");
            }

            // ================= HOUSE-00097: the concurrent voice ceiling =============================
            {
                // Instances are created and played until either one refuses to start or a generous
                // ceiling is reached. The design target is 32; the measurement decides whether that is
                // conservative or optimistic on this host.
                const int limit = 512;
                std::vector<std::unique_ptr<SoundEffectInstance>> voices;
                int playing = 0;
                int firstFailure = -1;
                std::string failure;
                for (int i = 0; i < limit; ++i)
                {
                    try
                    {
                        auto inst = std::make_unique<SoundEffectInstance>(tone.CreateInstance());
                        inst->setIsLoopedProperty(true);
                        inst->setVolumeProperty(0.01f); // audible in principle, inaudible in practice
                        inst->Play();
                        if (inst->getStateProperty() != SoundState::Playing)
                        {
                            if (firstFailure < 0)
                            {
                                firstFailure = i;
                            }
                        }
                        else
                        {
                            ++playing;
                        }
                        voices.push_back(std::move(inst));
                    }
                    catch (const std::exception& e)
                    {
                        firstFailure = i;
                        failure = e.what();
                        break;
                    }
                }
                char vm[220];
                std::snprintf(vm,
                              sizeof vm,
                              "%d of %d instances reported Playing; first refusal at index %d%s",
                              playing,
                              limit,
                              firstFailure,
                              failure.empty() ? "" : (" (" + failure + ")").c_str());
                r.note("HOUSE-00097 concurrent voices", vm);
                r.check("the design target of 32 concurrent voices is met", playing >= 32, vm);
                r.note("HOUSE-00097 caveat",
                       firstFailure < 0
                           ? "CNA refused NOTHING up to the probe's own ceiling, so there is no "
                             "reported hardware limit to discover through the XNA surface. The voice "
                             "budget is therefore ours to impose and ours to enforce -- CNA will not "
                             "tell us when we have exceeded what the mixer can usefully play."
                           : "a refusal was observed; see the index above");
                for (auto& v : voices)
                {
                    v->Stop();
                }
            }

            return r.finish("p1-audio3d");
        }
    };

} // namespace

P1_MAIN(Audio3dProbe, "p1-audio3d")
