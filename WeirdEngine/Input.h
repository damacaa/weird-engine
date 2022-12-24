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

	int m_mouseX = 0;
	int m_mouseY = 0;
	bool m_mouseHasBeenMoved = false;
	int m_deltaX = 0;
	int m_deltaY = 0;

	int* m_keyTable;
	int* m_mouseKeysTable;

	bool m_mouseWarp = true;
	bool m_firstMouseInput = true;

public:

	Input() {
		m_keyTable = new int[256]{ 0 };
		m_mouseKeysTable = new int[5]{ 0 };
	}

	~Input() {
		delete m_keyTable;
		delete m_mouseKeysTable;
	}

	static Input& GetInstance() {
		static Input* _instance = new Input();
		return *_instance;
	};

	enum class MouseButton {
		LeftClick = 0,
		MiddleClick = 1,
		RightClick = 2,
		WheelUp = 3,
		WheelDown = 4
	};

	int GetMouseX() { return m_mouseX; };
	int GetMouseY() { return m_mouseY; };
	float GetMouseDeltaX() { return (float)m_deltaX / (float)GLUT_SCREEN_WIDTH; };
	float GetMouseDeltaY() { return (float)m_deltaY / (float)GLUT_SCREEN_HEIGHT; };

	void SetMouseXY(int x, int y) {

		if (m_firstMouseInput) {
			m_firstMouseInput = false;
			m_mouseX = x;
			m_mouseY = y;
		}

		m_deltaX = x - m_mouseX;
		m_mouseX = x;

		m_deltaY = y - m_mouseY;
		m_mouseY = y;

		m_mouseHasBeenMoved = true;

		if (m_mouseWarp && (
			x < 100 || x > GLUT_SCREEN_WIDTH - 100 ||
			y < 100 || y > GLUT_SCREEN_HEIGHT - 100)) {

			m_mouseX = GLUT_SCREEN_WIDTH / 2;
			m_mouseY = GLUT_SCREEN_HEIGHT / 2;

			glutWarpPointer(GLUT_SCREEN_WIDTH / 2, GLUT_SCREEN_HEIGHT / 2);
		}
	}

	bool GetMouseButton(MouseButton button) { return m_mouseKeysTable[(int)button] > 0; }
	bool GetMouseButtonDown(MouseButton button) { return m_mouseKeysTable[(int)button] == 1; }
	bool GetMouseButtonUp(MouseButton button) { return m_mouseKeysTable[(int)button] == -1; }

	void HandleMouseButton(int button, int state) { m_mouseKeysTable[button] = -((2 * state) - 1); }

	bool GetKey(unsigned char key) { return m_keyTable[toupper(std::min(255, (int)key))] > 0; }
	bool GetKeyDown(unsigned char key) { return m_keyTable[toupper(std::min(255, (int)key))] == 1; }
	bool GetKeyUp(unsigned char key) { return m_keyTable[toupper(std::min(255, (int)key))] == -1; }

	void PressKey(unsigned char key) {
		m_keyTable[toupper(std::min(255, (int)key))]++;
	}

	void ReleaseKey(unsigned char key) {
		m_keyTable[toupper(std::min(255, (int)key))] = -1;
	}

	void Update() {

		for (size_t i = 0; i < 256; i++)
		{
			// Updates after first frame where button was down
			m_keyTable[i] = m_keyTable[i] == 1 ? 2 : m_keyTable[i];

			// Updates after first frame where button was up
			m_keyTable[i] = m_keyTable[i] == -1 ? 0 : m_keyTable[i];
		}

		for (size_t i = 0; i < 5; i++)
		{
			m_mouseKeysTable[i] = m_mouseKeysTable[i] == 1 ? 2 : m_mouseKeysTable[i];
			m_mouseKeysTable[i] = m_mouseKeysTable[i] == -1 ? 0 : m_mouseKeysTable[i];
		}

		if (!m_mouseHasBeenMoved) {
			m_deltaX = 0;
			m_deltaY = 0;
		}
		m_mouseHasBeenMoved = false;
	}
};

