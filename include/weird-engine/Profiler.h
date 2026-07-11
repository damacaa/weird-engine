#pragma once
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include "weird-engine/Logger.h"

namespace WeirdEngine
{

	struct ScopeStats
	{
		const char* name;
		int depth;
		double totalTimeMs = 0.0;
		int count = 0;
		int parentIndex = -1;
	};

	class Profiler
	{
	public:
		static Profiler& Get()
		{
			static Profiler p;
			return p;
		}

		bool isRecording() const
		{
			return m_recording;
		}

		void startRecording()
		{
			if (!m_recording)
			{
				m_recording = true;
				m_startTime = std::chrono::high_resolution_clock::now();
				m_stats.clear();
				m_currentIndex = 0;
				m_currentDepth = 0;
				WeirdEngine::Logger::log("Profiler started recording...");
			}
		}

		// Enable real-time mode: keeps recording and snapshots per-frame data.
		void enableRealtime()
		{
			m_pendingRealtimeEnable = true;
			m_pendingRealtimeDisable = false;
		}

		void disableRealtime()
		{
			m_pendingRealtimeDisable = true;
			m_pendingRealtimeEnable = false;
		}

		bool isRealtime() const { return m_realtimeMode; }

		double getUnaccountedThreshold() const { return m_unaccountedThresholdPct; }
		void setUnaccountedThreshold(double threshold) { m_unaccountedThresholdPct = threshold; }

		const std::vector<ScopeStats>& getLastFrameStats() const { return m_lastFrameStats; }

		void update()
		{
			if (m_pendingRealtimeEnable)
			{
				m_realtimeMode = true;
				m_recording = true;
				m_stats.clear();
				m_lastFrameStats.clear();
				m_currentIndex = 0;
				m_currentDepth = 0;
				m_pendingRealtimeEnable = false;
			}
			else if (m_pendingRealtimeDisable)
			{
				m_realtimeMode = false;
				m_recording = false;
				m_pendingRealtimeDisable = false;
			}

			if (!m_recording)
				return;

			if (m_realtimeMode)
			{
				// Snapshot last frame's timings, inject "Others" rows, then reset accumulators.
				m_lastFrameStats.clear();
				injectOthersRange(0, m_stats.size(), m_stats, m_lastFrameStats);
				for (auto& s : m_stats)
				{
					s.totalTimeMs = 0.0;
					s.count = 0;
				}
			}
			else
			{
				auto now = std::chrono::high_resolution_clock::now();
				std::chrono::duration<double> diff = now - m_startTime;
				if (diff.count() >= 10.0)
				{
					stopRecording();
				}
			}
			m_currentIndex = 0;
			m_currentDepth = 0;
		}

		void stopRecording()
		{
			m_recording = false;
			printReport();
		}

		void beginScope(const char* name)
		{
			if (!m_recording)
				return;

			int parentIdx = m_stack.empty() ? -1 : m_stack.back().statIndex;
			int statIndex = -1;
			if (m_currentIndex < m_stats.size() && m_stats[m_currentIndex].name == name && m_stats[m_currentIndex].parentIndex == parentIdx)
			{
				statIndex = m_currentIndex;
				m_currentIndex++;
			}
			else
			{
				bool found = false;
				for (size_t i = 0; i < m_stats.size(); ++i)
				{
					if (m_stats[i].name == name && m_stats[i].depth == m_currentDepth && m_stats[i].parentIndex == parentIdx)
					{
						statIndex = i;
						m_currentIndex = i + 1;
						found = true;
						break;
					}
				}
				if (!found)
				{
					m_stats.push_back({name, m_currentDepth, 0.0, 0, parentIdx});
					statIndex = m_stats.size() - 1;
					m_currentIndex = m_stats.size();
				}
			}

			m_stack.push_back({statIndex, std::chrono::high_resolution_clock::now()});
			m_currentDepth++;
		}

		void endScope()
		{
			if (!m_recording || m_stack.empty())
				return;

			auto now = std::chrono::high_resolution_clock::now();
			auto item = m_stack.back();
			m_stack.pop_back();
			m_currentDepth--;

			std::chrono::duration<double, std::milli> diff = now - item.startTime;
			m_stats[item.statIndex].totalTimeMs += diff.count();
			m_stats[item.statIndex].count++;
		}

	private:
		void injectOthersRange(size_t start, size_t end, const std::vector<ScopeStats>& inStats, std::vector<ScopeStats>& outStats)
		{
			size_t i = start;
			while (i < end)
			{
				const auto& stat = inStats[i];
				outStats.push_back(stat);

				size_t subEnd = i + 1;
				while (subEnd < end && inStats[subEnd].depth > stat.depth)
					subEnd++;

				if (subEnd > i + 1)
				{
					double childrenTotalMs = 0.0;
					for (size_t j = i + 1; j < subEnd; j++)
					{
						if (inStats[j].depth == stat.depth + 1)
							childrenTotalMs += inStats[j].totalTimeMs;
					}

					injectOthersRange(i + 1, subEnd, inStats, outStats);

					double unaccountedMs = stat.totalTimeMs - childrenTotalMs;
					double unaccountedPctOfScope = stat.totalTimeMs > 0.0 ? (unaccountedMs / stat.totalTimeMs) * 100.0 : 0.0;
					if (unaccountedPctOfScope > m_unaccountedThresholdPct)
					{
						outStats.push_back({"Others", stat.depth + 1, unaccountedMs, stat.count, stat.parentIndex});
					}
				}

				i = subEnd;
			}
		}

		void printReport()
		{
			std::ostringstream oss;
			oss << "\n=================================================================\n";
			oss << "                       PERFORMANCE REPORT\n";
			oss << "=================================================================\n";

			double totalFrameTimeMs = 0.0;
			int frameCount = 0;
			if (!m_stats.empty())
			{
				totalFrameTimeMs = m_stats[0].totalTimeMs;
				frameCount = m_stats[0].count;
			}

			if (frameCount == 0)
				frameCount = 1;

			oss << std::left << std::setw(40) << "Scope" << std::right << std::setw(15) << "Avg Time (ms)"
					  << std::setw(10) << "% Frame" << "\n";
			oss << "-----------------------------------------------------------------\n";

			printStatsRange(0, m_stats.size(), totalFrameTimeMs, oss);

			oss << "=================================================================\n";
			WeirdEngine::Logger::log(oss.str());
		}

		void printStatsRange(size_t start, size_t end, double totalFrameTimeMs, std::ostringstream& oss)
		{
			size_t i = start;
			while (i < end)
			{
				const auto& stat = m_stats[i];

				std::string indent(stat.depth * 4, ' ');
				std::string name = indent + stat.name;
				double avgTimeMs = stat.totalTimeMs / stat.count;
				double percentage = totalFrameTimeMs > 0.0 ? (stat.totalTimeMs / totalFrameTimeMs) * 100.0 : 0.0;

				if (stat.depth == 0)
					percentage = 100.0;

				oss << std::left << std::setw(40) << name << std::right << std::setw(15) << std::fixed
						  << std::setprecision(3) << avgTimeMs << std::setw(9) << std::fixed << std::setprecision(1)
						  << percentage << "%\n";

				// Find end of this stat's subtree
				size_t subEnd = i + 1;
				while (subEnd < end && m_stats[subEnd].depth > stat.depth)
					subEnd++;

				if (subEnd > i + 1)
				{
					// Sum direct children total times
					double childrenTotalMs = 0.0;
					for (size_t j = i + 1; j < subEnd; j++)
					{
						if (m_stats[j].depth == stat.depth + 1)
							childrenTotalMs += m_stats[j].totalTimeMs;
					}

					// Print children recursively
					printStatsRange(i + 1, subEnd, totalFrameTimeMs, oss);

					// If unaccounted time exceeds 1% of scope, print an "Other" row
					double unaccountedMs = stat.totalTimeMs - childrenTotalMs;
					double unaccountedPctOfScope = stat.totalTimeMs > 0.0 ? (unaccountedMs / stat.totalTimeMs) * 100.0 : 0.0;
					double unaccountedPct = totalFrameTimeMs > 0.0 ? (unaccountedMs / totalFrameTimeMs) * 100.0 : 0.0;
					if (unaccountedPctOfScope > m_unaccountedThresholdPct)
					{
						std::string otherIndent((stat.depth + 1) * 4, ' ');
						std::string otherName = otherIndent + "Others";
						double otherAvgMs = unaccountedMs / stat.count;
						oss << std::left << std::setw(40) << otherName << std::right << std::setw(15)
								  << std::fixed << std::setprecision(3) << otherAvgMs << std::setw(9) << std::fixed
								  << std::setprecision(1) << unaccountedPct << "%\n";
					}
				}

				i = subEnd;
			}
		}

		struct StackItem
		{
			int statIndex;
			std::chrono::high_resolution_clock::time_point startTime;
		};

		bool m_recording = false;
		bool m_realtimeMode = false;
		bool m_pendingRealtimeEnable = false;
		bool m_pendingRealtimeDisable = false;
		std::chrono::high_resolution_clock::time_point m_startTime;
		std::vector<ScopeStats> m_stats;
		std::vector<ScopeStats> m_lastFrameStats;
		std::vector<StackItem> m_stack;
		int m_currentIndex = 0;
		int m_currentDepth = 0;
		double m_unaccountedThresholdPct = 5.0;
	};

	class ProfilerScope
	{
	public:
		ProfilerScope(const char* name)
		{
			Profiler::Get().beginScope(name);
		}
		~ProfilerScope()
		{
			Profiler::Get().endScope();
		}
	};

} // namespace WeirdEngine

#define PROFILE_SCOPE(name) WeirdEngine::ProfilerScope profile_scope_##__LINE__(name)
#define PROFILE_FUNCTION() PROFILE_SCOPE(__FUNCTION__)
