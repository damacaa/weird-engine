#pragma once
#include "Math.h"
#include <iostream>
#include <string>

#include<glad/glad.h>
#include<GLFW/glfw3.h>




/// <summary>
/// Stores the state of every key in the keyboard and mouse.
/// States are:
#define RELEASED_THIS_FRAME        -1	/// -1 Button released this frame.
#define NOT_PRESSED                 0	/// 0 Button not pressed.
#define PRESSED_IN_CURRENT_FRAME    1	/// 1 Started pressing the button in current frame.
#define IS_PRESSED                  2	/// 2 Button has been pressed down for at least one previous frame. 
/// </summary>
class Input
{
private:


	int m_mouseX = 0;
	int m_mouseY = 0;
	bool m_mouseHasBeenMoved = false;
	int m_deltaX = 0;
	int m_deltaY = 0;

	GLFWwindow* m_window;
	int m_width, m_height;

	int* m_keyTable;
	int* m_mouseKeysTable;

	bool m_mouseWarp = true;
	bool m_firstMouseInput = true;

	Input() {
		m_keyTable = new int[256] { 0 };
		m_mouseKeysTable = new int[5] { 0 };
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

#pragma region MouseMovement



	static int GetMouseX() { return GetInstance().m_mouseX; };
	static int GetMouseY() { return GetInstance().m_mouseY; };
	static float GetMouseDeltaX() { return (float)GetInstance().m_deltaX / GetInstance().m_width; };
	static float GetMouseDeltaY() { return (float)GetInstance().m_deltaY / GetInstance().m_height; };

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
			x < 100 || x > instance.m_width - 100 ||
			y < 100 || y > instance.m_height - 100)) {

			int centerX = instance.m_width / 2;
			int centerY = instance.m_height / 2;

			instance.m_mouseX = centerX;
			instance.m_mouseY = centerY;

			//glutWarpPointer(GLUT_SCREEN_WIDTH / 2, GLUT_SCREEN_HEIGHT / 2);
			// Sets mouse cursor to the middle of the screen so that it doesn't end up roaming around
			glfwSetCursorPos(instance.m_window, centerX, centerY);
		}
	}

#pragma endregion

#pragma region MouseButtons

	static bool GetMouseButton(MouseButton button) { return GetInstance().m_mouseKeysTable[(int)button] > 0; }
	static bool GetMouseButtonDown(MouseButton button) { return GetInstance().m_mouseKeysTable[(int)button] == 1; }
	static bool GetMouseButtonUp(MouseButton button) { return GetInstance().m_mouseKeysTable[(int)button] == RELEASED_THIS_FRAME; }

	// WTF
	static void HandleMouseButton(int button, int state) { GetInstance().m_mouseKeysTable[button] = -((2 * state) - 1); }

#pragma endregion


#pragma region Keyboard

	static bool GetKey(unsigned char key) { return GetInstance().m_keyTable[toupper(std::min(255, (int)key))] > NOT_PRESSED; }
	static bool GetKeyDown(unsigned char key) { return GetInstance().m_keyTable[toupper(std::min(255, (int)key))] == PRESSED_IN_CURRENT_FRAME; }
	static bool GetKeyUp(unsigned char key) { return GetInstance().m_keyTable[toupper(std::min(255, (int)key))] == RELEASED_THIS_FRAME; }

	static void PressKey(unsigned char key) {
		GetInstance().m_keyTable[toupper(std::min(255, (int)key))]++;
	}

	static void ReleaseKey(unsigned char key) {
		GetInstance().m_keyTable[toupper(std::min(255, (int)key))] = -1;
	}

#pragma endregion



	static void Update(GLFWwindow* window, int width, int height) {

		auto& instance = GetInstance();
		instance.m_window = window;
		instance.m_width = width;
		instance.m_height = height;

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

