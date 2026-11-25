#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <weird-audio/SdfSong.h>
#include <weird-engine.h>
#include <weird-engine/shapes/NodeGraphCompiler.h>
#include <weird-engine/shapes/NodeGraphSerializer.h>

namespace MusicShapes
{
	using namespace WeirdEngine;
	using namespace WeirdEngine::SDF;
	using namespace WeirdEngine::WeirdAudio;

	struct LoadedSongAsset
	{
		std::string title;
		std::string description;
		std::string filePath;
		float tempo = 120.0f;
		MusicalScale scale = MusicalScale::PentatonicMinor;
		int rootMidi = 60;
		float defaultParam0 = 24.0f;
		float defaultParam1 = 1.0f;
		NodeGraph graph;
	};

	namespace detail
	{
		struct SongPresetMetadata
		{
			std::string title;
			std::string description;
			float tempo;
			MusicalScale scale;
			int rootMidi;
		};

		inline std::string formatStemToTitle(const std::string& stem)
		{
			std::string result;
			result.reserve(stem.size());
			bool capitalizeNext = true;
			for (unsigned char ch : stem)
			{
				if (ch == '_' || ch == '-')
				{
					if (!result.empty() && result.back() != ' ')
					{
						result.push_back(' ');
					}
					capitalizeNext = true;
				}
				else if (capitalizeNext)
				{
					result.push_back(static_cast<char>(std::toupper(ch)));
					capitalizeNext = false;
				}
				else
				{
					result.push_back(static_cast<char>(ch));
				}
			}
			return result.empty() ? std::string("Untitled Song") : result;
		}

		inline const std::unordered_map<std::string, SongPresetMetadata>& getKnownMetadata()
		{
			static const std::unordered_map<std::string, SongPresetMetadata> s_metadata = {
				{"midnight_pulse",
				 {"Midnight Pulse", "Midnight Pulse (Synthwave Beat)", 120.0f, MusicalScale::PentatonicMinor, 57}},
				{"silica_dreams",
				 {"Silica Dreams", "Silica Dreams (Ambient Chill)", 92.0f, MusicalScale::PentatonicMajor, 60}},
				{"velocity", {"Velocity", "Velocity (Electronic Melody)", 138.0f, MusicalScale::Dorian, 62}},
				{"vortex", {"Vortex", "Vortex (Hypnotic Rhythm)", 110.0f, MusicalScale::PentatonicMinor, 58}},
			};
			return s_metadata;
		}
	} // namespace detail

	inline std::vector<LoadedSongAsset> loadSongsFromAssets(const ResourceService& resources)
	{
		std::vector<LoadedSongAsset> assets;
		const std::filesystem::path songsDir(resources.assetPath("songs"));
		if (!std::filesystem::exists(songsDir) || !std::filesystem::is_directory(songsDir))
		{
			Logger::log("[MusicShapes] Songs directory not found: " + songsDir.string());
			return assets;
		}

		std::vector<std::filesystem::path> songFiles;
		for (const auto& entry : std::filesystem::directory_iterator(songsDir))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".weirdshape")
			{
				songFiles.push_back(entry.path());
			}
		}

		std::sort(songFiles.begin(), songFiles.end(), [](const std::filesystem::path& a, const std::filesystem::path& b)
				  { return a.filename().string() < b.filename().string(); });

		const auto& knownMeta = detail::getKnownMetadata();

		for (const auto& path : songFiles)
		{
			std::ifstream file(path);
			if (!file.is_open())
			{
				Logger::error("[MusicShapes] Failed to open song shape: " + path.string());
				continue;
			}

			WeirdEngine::json j;
			try
			{
				file >> j;
			}
			catch (const std::exception& e)
			{
				Logger::error("[MusicShapes] Failed to parse song shape JSON " + path.string() + ": " + e.what());
				continue;
			}

			LoadedSongAsset asset;
			if (!NodeGraphSerializer::deserialize(asset.graph, j))
			{
				Logger::error("[MusicShapes] Failed to deserialize song shape: " + path.string());
				continue;
			}

			asset.filePath = path.string();
			const auto& params = asset.graph.getParameters();
			asset.defaultParam0 = (params[0] > 0.0f) ? params[0] : 24.0f;
			asset.defaultParam1 = (params[1] > 0.0f) ? params[1] : 1.0f;

			const std::string stem = path.stem().string();
			if (auto it = knownMeta.find(stem); it != knownMeta.end())
			{
				asset.title = it->second.title;
				asset.description = it->second.description;
				asset.tempo = it->second.tempo;
				asset.scale = it->second.scale;
				asset.rootMidi = it->second.rootMidi;
			}
			else
			{
				asset.title = detail::formatStemToTitle(stem);
				asset.description = asset.title;
			}

			assets.push_back(std::move(asset));
		}

		return assets;
	}

	inline Expr evaluateSongShape(LoadedSongAsset& asset, const Vec2Expr& p)
	{
		return NodeGraphCompiler::evaluate(asset.graph, p);
	}

	inline std::shared_ptr<SdfSong> createSong(LoadedSongAsset& asset, std::optional<glm::vec2> center = std::nullopt)
	{
		Expr shapeExpr = NodeGraphCompiler::evaluate(asset.graph);
		auto song = SdfSong::create(asset.title, shapeExpr, asset.tempo, asset.scale, asset.rootMidi);
		if (center.has_value())
		{
			song->setCenter(*center);
		}
		song->setParameter(0, asset.defaultParam0);
		song->setParameter(1, asset.defaultParam1);
		return song;
	}
} // namespace MusicShapes
