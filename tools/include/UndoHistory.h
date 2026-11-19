#pragma once

#include <cstddef>
#include <imgui.h>
#include <utility>
#include <vector>

namespace WeirdEngine::Tools
{
	enum class UndoShortcutAction
	{
		None,
		Undo,
		Redo
	};

	/// Polls ImGui keyboard state for Ctrl+Z (Undo) and Ctrl+Shift+Z / Ctrl+Y (Redo).
	/// Must be called inside the ImGui frame (onCustomUI) so panel focus / WantCaptureKeyboard
	/// does not suppress editor undo/redo while text inputs remain protected by WantTextInput.
	inline UndoShortcutAction pollUndoShortcut(bool allowShortcuts = true)
	{
		if (!allowShortcuts)
		{
			return UndoShortcutAction::None;
		}

		const ImGuiIO& io = ImGui::GetIO();
		if (!io.KeyCtrl || io.KeyAlt || io.WantTextInput)
		{
			return UndoShortcutAction::None;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
		{
			return io.KeyShift ? UndoShortcutAction::Redo : UndoShortcutAction::Undo;
		}

		if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Y, false))
		{
			return UndoShortcutAction::Redo;
		}

		return UndoShortcutAction::None;
	}

	template <typename StateT> class UndoHistory
	{
	public:
		static constexpr size_t kMaxHistory = 64;

		bool canUndo() const
		{
			return !m_undoStack.empty();
		}

		bool canRedo() const
		{
			return !m_redoStack.empty();
		}

		bool isDirty() const
		{
			return !m_cleanReachable || m_revision != m_cleanRevision;
		}

		void markClean()
		{
			m_cleanRevision = m_revision;
			m_cleanReachable = true;
		}

		void markDirty()
		{
			m_cleanReachable = false;
			++m_revision;
		}

		void setDirtyState(bool dirty)
		{
			m_cleanRevision = 0;
			m_revision = dirty ? 1 : 0;
			m_cleanReachable = true;
		}

		void clear(bool dirty = false)
		{
			m_undoStack.clear();
			m_redoStack.clear();
			m_cleanRevision = 0;
			m_revision = dirty ? 1 : 0;
			m_cleanReachable = true;
		}

		void push(StateT previousState)
		{
			// Diverging from an undone branch makes any saved state ahead on the redo stack unreachable.
			if (m_cleanReachable && m_revision < m_cleanRevision)
			{
				m_cleanReachable = false;
			}

			m_undoStack.push_back(std::move(previousState));
			if (m_undoStack.size() > kMaxHistory)
			{
				m_undoStack.erase(m_undoStack.begin());
			}
			m_redoStack.clear();
			++m_revision;
		}

		bool undo(const StateT& currentState, StateT& outPreviousState)
		{
			if (m_undoStack.empty())
			{
				return false;
			}

			outPreviousState = std::move(m_undoStack.back());
			m_undoStack.pop_back();
			m_redoStack.push_back(currentState);
			if (m_redoStack.size() > kMaxHistory)
			{
				m_redoStack.erase(m_redoStack.begin());
			}
			--m_revision;
			return true;
		}

		bool redo(const StateT& currentState, StateT& outNextState)
		{
			if (m_redoStack.empty())
			{
				return false;
			}

			outNextState = std::move(m_redoStack.back());
			m_redoStack.pop_back();
			m_undoStack.push_back(currentState);
			if (m_undoStack.size() > kMaxHistory)
			{
				m_undoStack.erase(m_undoStack.begin());
			}
			++m_revision;
			return true;
		}

	private:
		std::vector<StateT> m_undoStack;
		std::vector<StateT> m_redoStack;
		int m_revision = 0;
		int m_cleanRevision = 0;
		bool m_cleanReachable = true;
	};
} // namespace WeirdEngine::Tools
