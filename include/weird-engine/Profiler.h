#pragma once
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <deque>
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
			m_pendingReportRecordingEnable = true;
			m_pendingRealtimeDisable = false;
			m_pendingRealtimeEnable = false;
			WeirdEngine::Logger::log("Profiler pending report recording...");
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

		const std::vector<ScopeStats>& getLastFrameStats() const 
		{ 
			if (m_paused && m_historyEnabled && !m_historyBuffer.empty())
			{
				if (m_playbackIndex >= 0 && m_playbackIndex < m_historyBuffer.size())
				{
					return m_historyBuffer[m_playbackIndex];
				}
			}
			return m_lastFrameStats; 
		}

		void setHistoryEnabled(bool enabled)
		{
			if (m_historyEnabled != enabled)
			{
				m_historyEnabled = enabled;
				if (!enabled)
				{
					m_historyBuffer.clear();
					m_playbackIndex = -1;
				}
			}
		}
		bool isHistoryEnabled() const { return m_historyEnabled; }
		int getHistoryCapturedCount() const { return (int)m_historyBuffer.size(); }
		int getHistoryCapacity() const { return m_historyCapacity; }

		void pause() { m_pendingPause = true; }
		void resume() { m_pendingResume = true; }
		bool isPaused() const { return m_paused; }

		int getPlaybackIndex() const { return m_playbackIndex; }
		void setPlaybackIndex(int idx) { m_playbackIndex = idx; }

		double getReportProgressSeconds() const
		{
			if (m_recording && !m_realtimeMode && !m_reportFinished && !m_paused)
			{
				auto now = std::chrono::high_resolution_clock::now();
				std::chrono::duration<double> diff = now - m_startTime;
				return diff.count();
			}
			return 0.0;
		}

		bool isReportFinished() const { return m_reportFinished; }
		bool isRecordingReport() const { return m_recording && !m_realtimeMode; }

		void update()
		{
			if (m_pendingReportRecordingEnable)
			{
				m_realtimeMode = false;
				m_recording = true;
				m_reportFinished = false;
				m_startTime = std::chrono::high_resolution_clock::now();
				m_stats.clear();
				m_lastFrameStats.clear();
				m_currentIndex = 0;
				m_currentDepth = 0;
				m_pendingReportRecordingEnable = false;
				WeirdEngine::Logger::log("Profiler started report recording...");
			}
			else if (m_pendingRealtimeEnable)
			{
				m_realtimeMode = true;
				m_recording = true;
				m_reportFinished = false;
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
				m_reportFinished = false;
				m_pendingRealtimeDisable = false;
			}

			if (m_pendingPause)
			{
				m_paused = true;
				m_playbackIndex = m_historyBuffer.empty() ? 0 : (int)m_historyBuffer.size() - 1;
				m_pendingPause = false;
			}
			else if (m_pendingResume)
			{
				m_paused = false;
				m_stats.clear(); // clear any partial accumulations
				m_currentIndex = 0;
				m_currentDepth = 0;
				m_pendingResume = false;
			}

			if (!m_recording || m_paused)
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

				if (m_historyEnabled)
				{
					m_historyBuffer.push_back(m_lastFrameStats);
					if (m_historyBuffer.size() > m_historyCapacity)
					{
						m_historyBuffer.pop_front();
					}
				}
			}
			else
			{
				// Update display stats every frame to show running average
				m_lastFrameStats.clear();
				injectOthersRange(0, m_stats.size(), m_stats, m_lastFrameStats);

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
			m_reportFinished = true;
			WeirdEngine::Logger::log("Profiler recording finished.");
		}

		void beginScope(const char* name)
		{
			if (!m_recording || m_paused)
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
			if (!m_recording || m_paused || m_stack.empty())
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



		struct StackItem
		{
			int statIndex;
			std::chrono::high_resolution_clock::time_point startTime;
		};

		bool m_recording = false;
		bool m_realtimeMode = false;
		bool m_pendingRealtimeEnable = false;
		bool m_pendingRealtimeDisable = false;
		bool m_pendingReportRecordingEnable = false;
		bool m_reportFinished = false;
		bool m_historyEnabled = false;
		bool m_paused = false;
		bool m_pendingPause = false;
		bool m_pendingResume = false;
		int m_historyCapacity = 300;
		int m_playbackIndex = -1;
		std::chrono::high_resolution_clock::time_point m_startTime;
		std::vector<ScopeStats> m_stats;
		std::vector<ScopeStats> m_lastFrameStats;
		std::deque<std::vector<ScopeStats>> m_historyBuffer;
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
