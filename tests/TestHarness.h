#pragma once

#include "weird-engine/Logger.h"
#include "weird-engine/math/MathExpressions.h"
#include "weird-engine/Scene.h"
#include "weird-engine/SceneManager.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

namespace WeirdTests
{
	inline void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	struct SnapshotFile
	{
		std::filesystem::path path;

		SnapshotFile()
		{
			static size_t sequence = 0;
			const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
			path = std::filesystem::temp_directory_path() /
				   ("weird-snapshot-test-" + std::to_string(tick) + "-" + std::to_string(sequence++) + ".weirdsnap");
		}

		~SnapshotFile()
		{
			std::error_code error;
			std::filesystem::remove(path, error);
		}
	};

	template <typename Base> class InspectableScene : public Base
	{
	public:
		WeirdEngine::Registry* registry = nullptr;
		WeirdEngine::ServiceProvider* services = nullptr;

	protected:
		void onStart(WeirdEngine::Registry& sceneRegistry, WeirdEngine::ServiceProvider& sceneServices) override
		{
			registry = &sceneRegistry;
			services = &sceneServices;
		}
	};

	struct FloorPlaneSdf : public WeirdEngine::IMathExpression
	{
		float getValue(const float* parameters) const override
		{
			return parameters[10];
		}

		std::string print() const override
		{
			return "y";
		}
	};

	struct TestCase
	{
		std::string name;
		std::function<void()> run;
	};

	inline int runTests(int argc, char** argv, const char* suiteName, const std::vector<TestCase>& tests)
	{
#if defined(_MSC_VER) && defined(_DEBUG)
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
		WeirdEngine::Logger::s_enableConsoleOutput = false;

		const std::string_view filter =
			(argc > 1 && argv[1] != nullptr) ? std::string_view(argv[1]) : std::string_view();
		size_t executed = 0;
		size_t failed = 0;

		for (const auto& test : tests)
		{
			if (!filter.empty() && test.name != filter)
				continue;

			++executed;
			std::cout << "[RUN ] " << test.name << '\n' << std::flush;
			try
			{
				test.run();
				std::cout << "[PASS] " << test.name << '\n' << std::flush;
			}
			catch (const std::exception& error)
			{
				++failed;
				std::cerr << "[FAIL] " << test.name << ": " << error.what() << '\n' << std::flush;
			}
			catch (...)
			{
				++failed;
				std::cerr << "[FAIL] " << test.name << ": unknown exception\n" << std::flush;
			}
		}

		if (executed == 0)
		{
			std::cerr << suiteName << ": no test matched filter '" << filter << "'.\n";
			return 1;
		}

		if (failed > 0)
		{
			std::cerr << suiteName << ": " << failed << " of " << executed << " test(s) failed.\n";
			return 1;
		}

		std::cout << suiteName << ": " << executed << " test(s) passed.\n";
		return 0;
	}
} // namespace WeirdTests
