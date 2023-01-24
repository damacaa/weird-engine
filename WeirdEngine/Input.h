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

public:

	enum class MouseButton {
		LeftClick = 0,
		MiddleClick = 1,
		RightClick = 2,
		WheelUp = 3,
		WheelDown = 4
	};

	static int GetMouseX() { return GetInstance().m_mouseX; };
	static int GetMouseY() { return GetInstance().m_mouseY; };
	static float GetMouseDeltaX() { return (float)GetInstance().m_deltaX / (float)GLUT_SCREEN_WIDTH; };
	static float GetMouseDeltaY() { return (float)GetInstance().m_deltaY / (float)GLUT_SCREEN_HEIGHT; };

	static void SetMouseXY(int x, int y) {

		auto& instance = GetInstance();

		if (instance.m_firstMouseInput) {
			instance.m_firstMouseInput = false;
			instance.m_mouseX = x;
			instance.m_mouseY = y;
		}

		instance.m_deltaX = x - instance.m_mouseX;
		instance.m_mouseX = x;

		instance.m_deltaY = y - instance.m_mouseY;
		instance.m_mouseY = y;

		instance.m_mouseHasBeenMoved = true;

		if (instance.m_mouseWarp && (
			x < 100 || x > GLUT_SCREEN_WIDTH - 100 ||
			y < 100 || y > GLUT_SCREEN_HEIGHT - 100)) {

			instance.m_mouseX = GLUT_SCREEN_WIDTH / 2;
			instance.m_mouseY = GLUT_SCREEN_HEIGHT / 2;

			glutWarpPointer(GLUT_SCREEN_WIDTH / 2, GLUT_SCREEN_HEIGHT / 2);
		}
	}

	static bool GetMouseButton(MouseButton button) { return GetInstance().m_mouseKeysTable[(int)button] > 0; }
	static bool GetMouseButtonDown(MouseButton button) { return GetInstance().m_mouseKeysTable[(int)button] == 1; }
	static bool GetMouseButtonUp(MouseButton button) { return GetInstance().m_mouseKeysTable[(int)button] == -1; }

	static void HandleMouseButton(int button, int state) { GetInstance().m_mouseKeysTable[button] = -((2 * state) - 1); }



	static bool GetKey(unsigned char key) { return GetInstance().m_keyTable[toupper(std::min(255, (int)key))] > 0; }
	static bool GetKeyDown(unsigned char key) { return GetInstance().m_keyTable[toupper(std::min(255, (int)key))] == 1; }
	static bool GetKeyUp(unsigned char key) { return GetInstance().m_keyTable[toupper(std::min(255, (int)key))] == -1; }

	static void PressKey(unsigned char key) {
		GetInstance().m_keyTable[toupper(std::min(255, (int)key))]++;
	}

	static void ReleaseKey(unsigned char key) {
		GetInstance().m_keyTable[toupper(std::min(255, (int)key))] = -1;
	}

	static void Update() {

		auto& instance = GetInstance();

		for (size_t i = 0; i < 256; i++)
		{
			// Updates after first frame where button was down
			instance.m_keyTable[i] = instance.m_keyTable[i] == 1 ? 2 : instance.m_keyTable[i];

			// Updates after first frame where button was up
			instance.m_keyTable[i] = instance.m_keyTable[i] == -1 ? 0 : instance.m_keyTable[i];
		}

		for (size_t i = 0; i < 5; i++)
		{
			instance.m_mouseKeysTable[i] = instance.m_mouseKeysTable[i] == 1 ? 2 : instance.m_mouseKeysTable[i];
			instance.m_mouseKeysTable[i] = instance.m_mouseKeysTable[i] == -1 ? 0 : instance.m_mouseKeysTable[i];
		}

		if (!instance.m_mouseHasBeenMoved) {
			instance.m_deltaX = 0;
			instance.m_deltaY = 0;
		}
		instance.m_mouseHasBeenMoved = false;
	}
};

