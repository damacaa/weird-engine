#pragma once
#include "Math.h"
#include <iostream>
#include <string>
#include <GL/glut.h>

/// <summary>
/// Stores the state of every key in the keyboard and mouse.
/// States are:
/// -1 Button released this frame.
/// 0 Button not pressed.
/// 1 Started pressing the button in current frame.
/// 2 Button has been pressed down for at least one previous frame. 
/// </summary>
class Input
{
	static int m_mouseX;
	static int m_mouseY;
	static bool m_mouseHasBeenMoved;
	static int m_deltaX;
	static int m_deltaY;

	static int m_keyTable[256];
	static int m_mouseKeysTable[5];

public:

	enum class MouseButton {
		LeftClick = 0,
		MiddleClick = 1,
		RightClick = 2,
		WheelUp = 3,
		WheelDown = 4
	};

	static int GetMouseX() { return m_mouseX; };
	static int GetMouseY() { return m_mouseY; };
	static float GetMouseDeltaX() { return (float)m_deltaX / (float)GLUT_SCREEN_WIDTH; };
	static float GetMouseDeltaY() { return (float)m_deltaY / (float)GLUT_SCREEN_HEIGHT; };

	static void SetMouseXY(int x, int y) {
		m_deltaX = x - m_mouseX;
		m_mouseX = x;

		m_deltaY = y - m_mouseY;
		m_mouseY = y;

		m_mouseHasBeenMoved = true;
	}

	static bool GetMouseButton(MouseButton button) { return m_mouseKeysTable[(int)button] > 0; }
	static bool GetMouseButtonDown(MouseButton button) { return m_mouseKeysTable[(int)button] == 1; }
	static bool GetMouseButtonUp(MouseButton button) { return m_mouseKeysTable[(int)button] == -1; }

	static void HandleMouseButton(int button, int state) { m_mouseKeysTable[button] = -((2 * state) - 1); }

	static bool GetKey(unsigned char key) { return m_keyTable[toupper(key)] > 0; }
	static bool GetKeyDown(unsigned char key) { return m_keyTable[toupper(key)] == 1; }
	static bool GetKeyUp(unsigned char key) { return m_keyTable[toupper(key)] == -1; }

	static void PressKey(unsigned char key) {
		m_keyTable[toupper(key)]++;
	}

	static void ReleaseKey(unsigned char key) {
		m_keyTable[toupper(key)] = -1;
	}

	static void Update() {
		for (size_t i = 0; i < 256; i++)
		{
			// Updates after first frame where button was down
			m_keyTable[i] = m_keyTable[i] == 1 ? 2 : m_keyTable[i];
			m_mouseKeysTable[i] = m_mouseKeysTable[i] == 1 ? 2 : m_mouseKeysTable[i];

			// Updates after first frame where button was up
			m_keyTable[i] = m_keyTable[i] == -1 ? 0 : m_keyTable[i];
			m_mouseKeysTable[i] = m_mouseKeysTable[i] == -1 ? 0 : m_mouseKeysTable[i];


			if (!m_mouseHasBeenMoved) {
				m_deltaX = 0;
				m_deltaY = 0;
			}

			m_mouseHasBeenMoved = false;
		}
	}
};

