#pragma once
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace WeirdEngine
{

	struct ScopeStats
	{
		const char* name;
		int depth;
		double totalTimeMs = 0.0;
		int count = 0;
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
				std::cout << "Profiler started recording..." << std::endl;
			}
		}

		void update()
		{
			if (!m_recording)
				return;

			auto now = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> diff = now - m_startTime;
			if (diff.count() >= 10.0)
			{
				stopRecording();
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

			int statIndex = -1;
			if (m_currentIndex < m_stats.size() && m_stats[m_currentIndex].name == name)
			{
				statIndex = m_currentIndex;
				m_currentIndex++;
			}
			else
			{
				bool found = false;
				for (size_t i = 0; i < m_stats.size(); ++i)
				{
					if (m_stats[i].name == name && m_stats[i].depth == m_currentDepth)
					{
						statIndex = i;
						m_currentIndex = i + 1;
						found = true;
						break;
					}
				}
				if (!found)
				{
					m_stats.push_back({name, m_currentDepth, 0.0, 0});
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
		void printReport()
		{
			std::cout << "\n=================================================================\n";
			std::cout << "                       PERFORMANCE REPORT\n";
			std::cout << "=================================================================\n";

			double totalFrameTimeMs = 0.0;
			int frameCount = 0;
			if (!m_stats.empty())
			{
				totalFrameTimeMs = m_stats[0].totalTimeMs;
				frameCount = m_stats[0].count;
			}

			if (frameCount == 0)
				frameCount = 1;

			std::cout << std::left << std::setw(40) << "Scope" << std::right << std::setw(15) << "Avg Time (ms)"
					  << std::setw(10) << "% Frame" << "\n";
			std::cout << "-----------------------------------------------------------------\n";

			for (const auto& stat : m_stats)
			{
				std::string indent(stat.depth * 4, ' ');
				std::string name = indent + stat.name;
				double avgTimeMs = stat.totalTimeMs / stat.count;
				double percentage = totalFrameTimeMs > 0.0 ? (stat.totalTimeMs / totalFrameTimeMs) * 100.0 : 0.0;

				if (stat.depth == 0)
					percentage = 100.0;

				std::cout << std::left << std::setw(40) << name << std::right << std::setw(15) << std::fixed
						  << std::setprecision(3) << avgTimeMs << std::setw(9) << std::fixed << std::setprecision(1)
						  << percentage << "%\n";
			}
			std::cout << "=================================================================\n\n";
		}

		struct StackItem
		{
			int statIndex;
			std::chrono::high_resolution_clock::time_point startTime;
		};

		bool m_recording = false;
		std::chrono::high_resolution_clock::time_point m_startTime;
		std::vector<ScopeStats> m_stats;
		std::vector<StackItem> m_stack;
		int m_currentIndex = 0;
		int m_currentDepth = 0;
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
