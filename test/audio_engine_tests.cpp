#include "audio_engine/audio_engine_pimpl.hpp"
#include "utility.hpp"

#include <gtest/gtest.h>

TEST(AudioEngine, decodesAndControlsSoundWithoutAnOutputDevice)
{
	AudioEnginePimpl engine(AudioOutputMode::NO_DEVICE);
	EXPECT_FALSE(engine.has_output());
	auto source = engine.create_source();

	EXPECT_NO_THROW(source.set_audio(
		Utility::get_audio("bounce.wav").string(), AudioLoadMode::DECODE));
	EXPECT_NO_THROW(source.set_gain(0.5f));
	EXPECT_NO_THROW(source.set_pitch(1.25f));
	EXPECT_NO_THROW(source.set_position({ 1.0f, 2.0f, 3.0f }));
	EXPECT_NO_THROW(source.set_loop(true));
	EXPECT_NO_THROW(source.play());
	EXPECT_NO_THROW(source.stop());
	EXPECT_NO_THROW(engine.set_listener_transform(
		{ 0.0f, 1.0f, 2.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }));
	EXPECT_NO_THROW(source.set_audio(
		Utility::get_audio("bounce.wav").string(), AudioLoadMode::STREAM));
}

TEST(AudioEngine, movedSourceRetainsItsLoadedSound)
{
	AudioEnginePimpl engine(AudioOutputMode::NO_DEVICE);
	auto original = engine.create_source();
	original.set_audio(Utility::get_audio("bounce.wav").string());

	AudioSource moved(std::move(original));
	EXPECT_NO_THROW(moved.play());
	EXPECT_NO_THROW(moved.stop());
}

TEST(AudioEngine, reportsMissingAudioFiles)
{
	AudioEnginePimpl engine(AudioOutputMode::NO_DEVICE);
	auto source = engine.create_source();
	EXPECT_THROW(source.set_audio("does-not-exist.wav"), std::runtime_error);
}
