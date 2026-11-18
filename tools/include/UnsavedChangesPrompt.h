#pragma once

#include <functional>
#include <imgui.h>
#include <string>
#include <utility>

namespace WeirdEngine::Tools
{
	/// Reusable ImGui "Unsaved Changes" modal for guarding scene switches,
	/// file loads/resets, tool navigation, and application quit actions.
	class UnsavedChangesPrompt
	{
	public:
		/// Executes `action` immediately if `isDirty` is false; otherwise opens
		/// the Save / Discard / Cancel prompt before running `action`.
		void request(bool isDirty, std::string itemName, std::function<void()> action, std::string customMessage = {})
		{
			if (!isDirty)
			{
				if (action)
				{
					action();
				}
				return;
			}

			m_itemName = itemName.empty() ? "Untitled" : std::move(itemName);
			m_customMessage = std::move(customMessage);
			m_pendingAction = std::move(action);
			m_waitingForSave = false;
			m_openRequested = true;
		}

		/// Completes or cancels a deferred action waiting on an async Save dialog.
		void notifySaveFinished(bool saved)
		{
			if (!m_waitingForSave)
			{
				return;
			}
			m_waitingForSave = false;
			auto action = std::move(m_pendingAction);
			m_pendingAction = nullptr;
			if (saved && action)
			{
				action();
			}
		}

		void clear()
		{
			m_pendingAction = nullptr;
			m_waitingForSave = false;
			m_openRequested = false;
			m_customMessage.clear();
		}

		bool isOpenOrPending() const
		{
			return m_pendingAction != nullptr || m_openRequested;
		}

		/// Call at root UI scope once per frame. `trySave()` returns true when saved
		/// synchronously or false when an async Save As dialog was opened (which must
		/// later invoke `notifySaveFinished(saved)`).
		template <typename SaveFn> void render(const SaveFn& trySave, const std::function<void()>& onDiscard = nullptr)
		{
			if (m_openRequested)
			{
				ImGui::OpenPopup("Unsaved Changes##WeirdEditorPrompt");
				m_openRequested = false;
			}

			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			if (ImGui::BeginPopupModal("Unsaved Changes##WeirdEditorPrompt", nullptr,
									   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
			{
				if (!m_customMessage.empty())
				{
					ImGui::TextWrapped("%s", m_customMessage.c_str());
				}
				else
				{
					ImGui::TextWrapped("Do you want to save changes to \"%s\" before leaving?", m_itemName.c_str());
				}
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.52f, 0.28f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.65f, 0.36f, 1.0f));
				if (ImGui::Button("Save", ImVec2(100.0f, 0.0f)))
				{
					ImGui::CloseCurrentPopup();
					m_waitingForSave = true;
					if (trySave())
					{
						notifySaveFinished(true);
					}
				}
				ImGui::PopStyleColor(2);

				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.62f, 0.22f, 0.22f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.76f, 0.28f, 0.28f, 1.0f));
				if (ImGui::Button("Discard", ImVec2(100.0f, 0.0f)))
				{
					ImGui::CloseCurrentPopup();
					m_waitingForSave = false;
					if (onDiscard)
					{
						onDiscard();
					}
					auto action = std::move(m_pendingAction);
					m_pendingAction = nullptr;
					if (action)
					{
						action();
					}
				}
				ImGui::PopStyleColor(2);

				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape))
				{
					ImGui::CloseCurrentPopup();
					m_waitingForSave = false;
					m_pendingAction = nullptr;
				}
				ImGui::EndPopup();
			}
		}

	private:
		std::string m_itemName = "Untitled";
		std::string m_customMessage;
		std::function<void()> m_pendingAction;
		bool m_openRequested = false;
		bool m_waitingForSave = false;
	};
} // namespace WeirdEngine::Tools
