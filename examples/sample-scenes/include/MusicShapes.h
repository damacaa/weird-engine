#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <weird-audio/SdfSong.h>
#include <weird-engine.h>

namespace MusicShapes
{
	using namespace WeirdEngine;
	using namespace WeirdEngine::SDF;
	using namespace WeirdEngine::WeirdAudio;

	// Procedural SDF Shape generator for Song 1 (Synthwave Star / Eclipse)
	inline Expr getSongShape01(Vec2Expr p)
	{
		Expr star = sdStar(p, Expr(var(0)), Expr(var(0)) * 0.2f, 12.0f, Expr(var(1)) * 5.0f);
		Expr eclipseX = sin(Expr(var(1)) * time() * 0.7f) * (Expr(var(0)) * 1.0f);
		Vec2Expr moonCenter = {eclipseX, Expr(0.0f)};
		Expr moon = sdCircle(p - moonCenter, Expr(var(0)) * 0.42f);
		return sdfSubtract(star, moon);
	}

	// Procedural SDF Shape generator for Song 2 (Ambient Wave Bubble / Float)
	inline Expr getSongShape02(Vec2Expr p)
	{
		Expr wave = sdSineWave(p, Expr(var(0)) * 0.22f, 0.08f, Expr(var(1)) * 1.5f, 0.0f);
		Expr bubbleY = sin(Expr(var(1)) * time() * 2.0f) * (Expr(var(0)) * 0.25f);
		Vec2Expr bubbleCenter = {Expr(0.0f), bubbleY};
		Expr bubble = sdCircle(p - bubbleCenter, Expr(var(0)) * 0.45f);
		return sdfSmoothUnion(wave, bubble, Expr(var(0)) * 0.15f);
	}

	// Procedural SDF Shape generator for Song 3 (Velocity Star Shuriken / Ninjas)
	inline Expr getSongShape03(Vec2Expr p)
	{
		Expr theta = Expr(var(1)) * time() * 1.6f;
		Vec2Expr rotP = {p.x * cos(theta) - p.y * sin(theta), p.x * sin(theta) + p.y * cos(theta)};
		Expr shuriken = sdStar(rotP, Expr(var(0)) * 0.75f, Expr(var(0)) * 0.42f, 4.0f, 0.0f);
		Expr centerHole = sdBox(rotate(p, theta * -1.0f), Vec2Expr(var(0) * 0.5f, var(0) * 0.5f));
		return sdfSubtract(shuriken, centerHole);
	}

	// Procedural SDF Shape generator for Song 4 (Hypnotic Rhythm / Vortex)
	inline Expr getSongShape04(Vec2Expr p)
	{
		Expr finalShape = SDF::sdCircle(
			SDF::repeat(SDF::rotate(SDF::translate(p, Vec2Expr(1000.0f, 0.0f)), (time() * var(1) * 0.5f)), 20.0f),
			(var(0) * 0.3f));
		return finalShape;
	}

	inline Expr getSongShape(size_t index, Vec2Expr p)
	{
		switch (index)
		{
			case 0:
				return getSongShape01(p);
			case 1:
				return getSongShape02(p);
			case 2:
				return getSongShape03(p);
			case 3:
			default:
				return getSongShape04(p);
		}
	}

	struct SongDefinition
	{
		std::string title;
		std::string description;
		float tempo;
		MusicalScale scale;
		int rootMidi;
		float defaultParam0;
		float defaultParam1;
		Expr (*shapeFunc)(Vec2Expr);
	};

	inline const std::array<SongDefinition, 4>& getSongDefinitions()
	{
		static const std::array<SongDefinition, 4> s_definitions = {
			SongDefinition{"Midnight Pulse", "Midnight Pulse (Synthwave Beat)", 120.0f, MusicalScale::PentatonicMinor,
						   57, 26.0f, 1.0f, getSongShape01},
			SongDefinition{"Silica Dreams", "Silica Dreams (Ambient Chill)", 92.0f, MusicalScale::PentatonicMajor, 60,
						   24.0f, 0.8f, getSongShape02},
			SongDefinition{"Velocity", "Velocity (Electronic Melody)", 138.0f, MusicalScale::Dorian, 62, 22.0f, 1.2f,
						   getSongShape03},
			SongDefinition{"Vortex", "Vortex (Hypnotic Rhythm)", 110.0f, MusicalScale::PentatonicMinor, 58, 26.0f, 1.0f,
						   getSongShape04},
		};
		return s_definitions;
	}

	inline std::shared_ptr<SdfSong> createSong(size_t index, std::optional<glm::vec2> center = std::nullopt)
	{
		const auto& defs = getSongDefinitions();
		if (index >= defs.size())
		{
			return nullptr;
		}

		const auto& def = defs[index];
		auto song = SdfSong::create(def.title, def.shapeFunc(point()), def.tempo, def.scale, def.rootMidi);
		if (center.has_value())
		{
			song->setCenter(*center);
		}
		song->setParameter(0, def.defaultParam0);
		song->setParameter(1, def.defaultParam1);
		return song;
	}

	inline std::vector<std::shared_ptr<SdfSong>> createAllSongs(std::optional<glm::vec2> center = std::nullopt)
	{
		const auto& defs = getSongDefinitions();
		std::vector<std::shared_ptr<SdfSong>> songs;
		songs.reserve(defs.size());
		for (size_t i = 0; i < defs.size(); ++i)
		{
			songs.push_back(createSong(i, center));
		}
		return songs;
	}
} // namespace MusicShapes
