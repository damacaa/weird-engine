#include "weird-engine/Logger.h"
#include <iostream>
#include <imgui.h>

namespace WeirdEngine
{
    std::vector<LogMessage> Logger::s_messages;
    std::mutex Logger::s_mutex;

    void Logger::log(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_messages.push_back({LogLevel::Info, message});
        std::cout << "[INFO] " << message << std::endl;
    }

    void Logger::warning(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_messages.push_back({LogLevel::Warning, message});
        std::cout << "[WARN] " << message << std::endl;
    }

    void Logger::error(const std::string& message)
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_messages.push_back({LogLevel::Error, message});
        std::cerr << "[ERROR] " << message << std::endl;
    }

    void Logger::drawImGuiConsole()
    {
        std::lock_guard<std::mutex> lock(s_mutex);

        ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        for (const auto& msg : s_messages)
        {
            ImVec4 color;
            switch (msg.level)
            {
                case LogLevel::Info:    color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break; // White
                case LogLevel::Warning: color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); break; // Yellow
                case LogLevel::Error:   color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); break; // Red
            }
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(msg.message.c_str());
            ImGui::PopStyleColor();
        }

        // Auto-scroll to bottom
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
    }
}
