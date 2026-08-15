#include "weird-engine/Logger.h"
#include <iostream>
#ifndef WEIRD_DISABLE_IMGUI
#include <imgui.h>
#endif

namespace WeirdEngine
{
	std::vector<LogMessage> Logger::s_messages;
	std::mutex Logger::s_mutex;
	bool Logger::s_enableConsoleOutput = true;

	void Logger::log(const std::string& message)
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		s_messages.push_back({LogLevel::Info, message});
		if (s_messages.size() > 1000)
			s_messages.erase(s_messages.begin());
		if (s_enableConsoleOutput)
			std::cout << "[INFO] " << message << std::endl;
	}

	void Logger::warning(const std::string& message)
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		s_messages.push_back({LogLevel::Warning, message});
		if (s_messages.size() > 1000)
			s_messages.erase(s_messages.begin());
		if (s_enableConsoleOutput)
			std::cout << "[WARN] " << message << std::endl;
	}

	void Logger::error(const std::string& message)
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		s_messages.push_back({LogLevel::Error, message});
		if (s_messages.size() > 1000)
			s_messages.erase(s_messages.begin());
		if (s_enableConsoleOutput)
			std::cerr << "[ERROR] " << message << std::endl;
	}

	void Logger::clear()
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		s_messages.clear();
	}

	void Logger::copyToClipboard()
	{
#ifndef WEIRD_DISABLE_IMGUI
		std::lock_guard<std::mutex> lock(s_mutex);
		std::string allText;
		for (const auto& msg : s_messages)
		{
			switch (msg.level)
			{
				case LogLevel::Info:
					allText += "[INFO] ";
					break;
				case LogLevel::Warning:
					allText += "[WARN] ";
					break;
				case LogLevel::Error:
					allText += "[ERROR] ";
					break;
			}
			allText += msg.message;
			allText += "\n";
		}
		ImGui::SetClipboardText(allText.c_str());
#endif
	}

	void Logger::drawImGuiConsole()
	{
#ifndef WEIRD_DISABLE_IMGUI
		if (ImGui::Button("Copy to Clipboard"))
		{
			copyToClipboard();
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
		{
			clear();
		}
		ImGui::SameLine();
		ImGui::Checkbox("Print to std::cout", &s_enableConsoleOutput);

		ImGui::Separator();

		std::lock_guard<std::mutex> lock(s_mutex);

		ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, 0);

		ImGui::PushTextWrapPos(0.0f);
		for (const auto& msg : s_messages)
		{
			ImVec4 color;
			switch (msg.level)
			{
				case LogLevel::Info:
					color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
					break; // White
				case LogLevel::Warning:
					color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
					break; // Yellow
				case LogLevel::Error:
					color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
					break; // Red
			}
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(msg.message.c_str());
			ImGui::PopStyleColor();
		}
		ImGui::PopTextWrapPos();

		// Auto-scroll to bottom
		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		{
			ImGui::SetScrollHereY(1.0f);
		}

		ImGui::EndChild();
#endif
	}
} // namespace WeirdEngine
