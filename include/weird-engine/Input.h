#pragma once

#include <iostream>
#include <string>

// Replaced glad and GLFW with SDL3.
// Note: If you use OpenGL with SDL, you'll still need a GL loader like glad,
// but it's not required for the input handling itself.
#include <SDL/include/SDL3/SDL.h>
// #include <SDL/src/events/SDL_mouse_c.h>

/// <summary>
/// Stores the state of every key in the keyboard and mouse.
/// States are:
#define RELEASED_THIS_FRAME -1 /// Button released this frame.
#define NOT_PRESSED 0 /// Button not pressed.
#define IS_PRESSED 1 /// Button has been pressed down for at least one previous frame.
#define FIRST_PRESSED 2 /// Started pressing the button in current frame.
/// </summary>

// Replaced hardcoded key count with SDL's constant for the number of scancodes.
#define SUPPORTED_KEYS SDL_NUM_SCANCODES

namespace WeirdEngine
{
	class Input
	{
	private:
		// Mouse position (now float, as SDL3 provides float precision)
		float m_mouseX = 0;
		float m_mouseY = 0;

		// Mouse position difference since last frame
		float m_deltaX = 0;
		float m_deltaY = 0;

		// SDL window
		SDL_Window* m_window;

		// Window size
		int m_width, m_height;

		// Store the state of every key and mouse button
		int* m_keyTable;
		int* m_mouseKeysTable;

		Input()
		{
			// Use SDL_NUM_SCANCODES for the key table size
			m_keyTable = new int[SDL_SCANCODE_COUNT] { 0 };
			m_mouseKeysTable = new int[5] { 0 };
		}

		~Input()
		{
			delete[] m_keyTable;
			delete[] m_mouseKeysTable;
		}

		// Singleton pattern remains the same
		static Input& getInstance()
		{
			static Input _instance; // Modern C++ handles static local initialization safely
			return _instance;
		};

		// Private update method, contains the core logic
		void pollEventsAndUpdateTables();
		void handleEventImpl(SDL_Event& event);

	public:
		enum MouseButton
		{
			LeftClick = 0,
			MiddleClick = 1,
			RightClick = 2,
			WheelUp = 3,
			WheelDown = 4
		};

		// The KeyCode enum is now mapped to SDL_Scancode values.
		// This represents the physical key on the keyboard.
		enum KeyCode
		{
			// Letters
			A = SDL_SCANCODE_A,
			B = SDL_SCANCODE_B,
			C = SDL_SCANCODE_C,
			D = SDL_SCANCODE_D,
			E = SDL_SCANCODE_E,
			F = SDL_SCANCODE_F,
			G = SDL_SCANCODE_G,
			H = SDL_SCANCODE_H,
			I = SDL_SCANCODE_I,
			J = SDL_SCANCODE_J,
			K = SDL_SCANCODE_K,
			L = SDL_SCANCODE_L,
			M = SDL_SCANCODE_M,
			N = SDL_SCANCODE_N,
			O = SDL_SCANCODE_O,
			P = SDL_SCANCODE_P,
			Q = SDL_SCANCODE_Q,
			R = SDL_SCANCODE_R,
			S = SDL_SCANCODE_S,
			T = SDL_SCANCODE_T,
			U = SDL_SCANCODE_U,
			V = SDL_SCANCODE_V,
			W = SDL_SCANCODE_W,
			X = SDL_SCANCODE_X,
			Y = SDL_SCANCODE_Y,
			Z = SDL_SCANCODE_Z,

			// Digits
			Num0 = SDL_SCANCODE_0,
			Num1 = SDL_SCANCODE_1,
			Num2 = SDL_SCANCODE_2,
			Num3 = SDL_SCANCODE_3,
			Num4 = SDL_SCANCODE_4,
			Num5 = SDL_SCANCODE_5,
			Num6 = SDL_SCANCODE_6,
			Num7 = SDL_SCANCODE_7,
			Num8 = SDL_SCANCODE_8,
			Num9 = SDL_SCANCODE_9,

			// Special keys
			Space = SDL_SCANCODE_SPACE,
			Enter = SDL_SCANCODE_RETURN,
			Tab = SDL_SCANCODE_TAB,
			Backspace = SDL_SCANCODE_BACKSPACE,
			Esc = SDL_SCANCODE_ESCAPE,
			Up = SDL_SCANCODE_UP,
			Down = SDL_SCANCODE_DOWN,
			Left = SDL_SCANCODE_LEFT,
			Right = SDL_SCANCODE_RIGHT,
			F1 = SDL_SCANCODE_F1,
			F2 = SDL_SCANCODE_F2,
			F3 = SDL_SCANCODE_F3,
			F4 = SDL_SCANCODE_F4,
			F5 = SDL_SCANCODE_F5,
			F6 = SDL_SCANCODE_F6,
			F7 = SDL_SCANCODE_F7,
			F8 = SDL_SCANCODE_F8,
			F9 = SDL_SCANCODE_F9,
			F10 = SDL_SCANCODE_F10,
			F11 = SDL_SCANCODE_F11,
			F12 = SDL_SCANCODE_F12,

			LeftShift = SDL_SCANCODE_LSHIFT,
			LeftCtrl = SDL_SCANCODE_LCTRL,
			LeftAlt = SDL_SCANCODE_LALT
		};

#pragma region MouseMovement

		static float GetMouseX() { return getInstance().m_mouseX; };
		static float GetMouseY() { return getInstance().m_mouseY; };
		static float GetMouseDeltaX() { return getInstance().m_deltaX / getInstance().m_width; };
		static float GetMouseDeltaY() { return getInstance().m_deltaY / getInstance().m_height; };

		static void SetMousePosition(float x, float y)
		{
			SDL_WarpMouseInWindow(getInstance().m_window, x, y);
			// After warping, update internal state to prevent a large delta on the next frame
			auto& instance = getInstance();
			instance.m_mouseX = x;
			instance.m_mouseY = y;
		}

		// Shows mouse cursor and allows it to leave the window.
		static void ShowMouse()
		{
			// SDL_SetRelativeMouseMode(false);
		}

		// Hides mouse cursor and locks it to the window (ideal for 3D camera control).
		static void HideMouse()
		{
			// SDL_SetRelativeMouseMode(true);
		}

#pragma endregion

#pragma region MouseButtons

		static bool GetMouseButton(MouseButton button)
		{
			return getInstance().m_mouseKeysTable[(int)button] >= IS_PRESSED;
		}

		static bool GetMouseButtonDown(MouseButton button)
		{
			return getInstance().m_mouseKeysTable[(int)button] == FIRST_PRESSED;
		}

		static bool GetMouseButtonUp(MouseButton button)
		{
			return getInstance().m_mouseKeysTable[(int)button] == RELEASED_THIS_FRAME;
		}

#pragma endregion

#pragma region Keyboard

		static bool GetKey(KeyCode key)
		{
			return getInstance().m_keyTable[key] >= IS_PRESSED;
		}

		// NOTE: Signature changed from unsigned char to KeyCode for consistency and correctness.
		// Use Input::W instead of 'W'.
		static bool GetKeyDown(KeyCode key)
		{
			return getInstance().m_keyTable[key] == FIRST_PRESSED;
		}

		// NOTE: Signature changed from unsigned char to KeyCode.
		static bool GetKeyUp(KeyCode key)
		{
			return getInstance().m_keyTable[key] == RELEASED_THIS_FRAME;
		}

#pragma endregion

	public:
		// Main update function, to be called once per frame.
		static void update(SDL_Window* window)
		{
			auto& instance = getInstance();
			instance.m_window = window;
			SDL_GetWindowSize(window, &instance.m_width, &instance.m_height);
			instance.pollEventsAndUpdateTables();
		}

		// Main update function, to be called once per frame.
		static void handleEvent(SDL_Event& event)
		{
			auto& instance = getInstance();
			// Handle specific events if needed, e.g., mouse wheel scrolling
			instance.handleEventImpl(event);
		}
	};

	inline void Input::handleEventImpl(SDL_Event& event)
	{

		switch (event.type)
		{
			// Handle mouse wheel separately as it's an event, not a persistent state.
		case SDL_EVENT_MOUSE_WHEEL:
			if (event.wheel.y > 0)
			{
				m_mouseKeysTable[(int)MouseButton::WheelUp] = FIRST_PRESSED;
			}
			else if (event.wheel.y < 0)
			{
				m_mouseKeysTable[(int)MouseButton::WheelDown] = FIRST_PRESSED;
			}
			break;
			// We can also handle quit events here if desired, though that's typically
			// done in the main application loop.
			// case SDL_EVENT_QUIT:
			//     ...
			//     break;
		}
	}

	// Implementation of the private update logic
	inline void Input::pollEventsAndUpdateTables()
	{
		// Reset states that only last for one frame (e.g., released, first press, wheel)
		// This loop transitions (FIRST_PRESSED -> IS_PRESSED) and (RELEASED_THIS_FRAME -> NOT_PRESSED)
		for (int i = 0; i < SDL_SCANCODE_COUNT; ++i)
		{
			if (m_keyTable[i] == FIRST_PRESSED)
			{
				m_keyTable[i] = IS_PRESSED;
			}
			else if (m_keyTable[i] == RELEASED_THIS_FRAME)
			{
				m_keyTable[i] = NOT_PRESSED;
			}
		}
		for (int i = 0; i < 5; ++i)
		{
			if (m_mouseKeysTable[i] == FIRST_PRESSED)
			{
				m_mouseKeysTable[i] = IS_PRESSED;
			}
			else if (m_mouseKeysTable[i] == RELEASED_THIS_FRAME)
			{
				m_mouseKeysTable[i] = NOT_PRESSED;
			}
		}
		// Mouse wheel is event-driven, so reset its state each frame before polling.
		m_mouseKeysTable[(int)MouseButton::WheelUp] = NOT_PRESSED;
		m_mouseKeysTable[(int)MouseButton::WheelDown] = NOT_PRESSED;
		m_deltaX = 0;
		m_deltaY = 0;

		// SDL's event loop. We need to process events to update states.

		// Now, poll the current state of all keys and buttons for this frame.
		// SDL_PumpEvents() is called inside SDL_PollEvent, so all states are up-to-date.

		// --- MOUSE ---
		SDL_GetMouseState(&m_mouseX, &m_mouseY);
		SDL_GetRelativeMouseState(&m_deltaX, &m_deltaY);

		const Uint32 mouseState = SDL_GetMouseState(NULL, NULL);

		// Map SDL mouse buttons to our table
		int sdlButtons[3] = { SDL_BUTTON_LMASK, SDL_BUTTON_RMASK, SDL_BUTTON_MMASK };
		int ourButtons[3] = { (int)MouseButton::LeftClick, (int)MouseButton::RightClick, (int)MouseButton::MiddleClick };

		for (int i = 0; i < 3; i++)
		{
			int current = (mouseState & sdlButtons[i]) ? IS_PRESSED : NOT_PRESSED;
			int previous = m_mouseKeysTable[ourButtons[i]];

			if (current == IS_PRESSED && (previous == NOT_PRESSED || previous == RELEASED_THIS_FRAME))
			{
				m_mouseKeysTable[ourButtons[i]] = FIRST_PRESSED;
			}
			else if (current == NOT_PRESSED && (previous == IS_PRESSED || previous == FIRST_PRESSED))
			{
				m_mouseKeysTable[ourButtons[i]] = RELEASED_THIS_FRAME;
			}
		}

		// --- KEYBOARD ---
		// 1. Declare an integer to hold the number of keys.
		int numScancodes = 0;

		// 2. Declare a pointer to hold the returned keyboard state array.
		//    It must be a `const Uint8*`.
		const bool* keyboardState = SDL_GetKeyboardState(&numScancodes);

		// It's good practice to check if the call succeeded, though it rarely fails.
		if (!keyboardState)
		{
			// Handle error, maybe log it.
			// SDL_GetError() might have more info.
			return;
		}

		for (int i = 0; i < SDL_SCANCODE_COUNT; i++)
		{
			int current = keyboardState[i] ? IS_PRESSED : NOT_PRESSED;
			int previous = m_keyTable[i];

			if (current == IS_PRESSED && (previous == NOT_PRESSED || previous == RELEASED_THIS_FRAME))
			{
				m_keyTable[i] = FIRST_PRESSED;
			}
			else if (current == NOT_PRESSED && (previous == IS_PRESSED || previous == FIRST_PRESSED))
			{
				m_keyTable[i] = RELEASED_THIS_FRAME;
			}
		}
	}
}