#pragma once
#include "Math.h"
#include <iostream>
#include <string>

#include<glad/glad.h>
#include<GLFW/glfw3.h>




/// <summary>
/// Stores the state of every key in the keyboard and mouse.
/// States are:
#define RELEASED_THIS_FRAME        -1	/// Button released this frame.
#define NOT_PRESSED                 0	/// Button not pressed.
#define IS_PRESSED                  1	/// Button has been pressed down for at least one previous frame. 
#define FIRST_PRESSED               2	/// Started pressing the button in current frame.
/// </summary>

#define SUPPORTED_KEYS			  342

class Input
{
private:

	// Mouse position
	double m_mouseX = 0;
	double m_mouseY = 0;

	// Mouse position difference since last frame
	double m_deltaX = 0;
	double m_deltaY = 0;

	// Mouse has been moved since last frame
	bool m_mouseHasBeenMoved = false;

	// OpenGL window
	GLFWwindow* m_window;

	// Window size
	int m_width, m_height;

	// Store the state of every key and mouse button
	int* m_keyTable;
	int* m_mouseKeysTable;


	Input() {
		m_keyTable = new int[SUPPORTED_KEYS] { 0 };
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

	enum MouseButton {
		LeftClick = 0,
		MiddleClick = 1,
		RightClick = 2,
		WheelUp = 3,
		WheelDown = 4
	};

	enum KeyCode
	{
		// Letters
		A = 'A',
		B = 'B',
		C = 'C',
		D = 'D',
		E = 'E',
		F = 'F',
		G = 'G',
		H = 'H',
		I = 'I',
		J = 'J',
		K = 'K',
		L = 'L',
		M = 'M',
		N = 'N',
		O = 'O',
		P = 'P',
		Q = 'Q',
		R = 'R',
		S = 'S',
		T = 'T',
		U = 'U',
		V = 'V',
		W = 'W',
		X = 'X',
		Y = 'Y',
		Z = 'Z',

		// Digits
		Num0 = '0',
		Num1 = '1',
		Num2 = '2',
		Num3 = '3',
		Num4 = '4',
		Num5 = '5',
		Num6 = '6',
		Num7 = '7',
		Num8 = '8',
		Num9 = '9',

		// Special keys
		Space = ' ',
		Enter = GLFW_KEY_ENTER,
		Tab = 9,
		Backspace = GLFW_KEY_BACKSPACE,
		Esc = GLFW_KEY_ESCAPE,
		Up = 128,
		Down = 129,
		Left = 130,
		Right = 131,
		F1 = 132,
		F2 = 133,
		F3 = 134,
		F4 = 135,
		F5 = 136,
		F6 = 137,
		F7 = 138,
		F8 = 139,
		F9 = 140,
		F10 = 141,
		F11 = 142,
		F12 = 143,

		LeftShift = GLFW_KEY_LEFT_SHIFT,
		LeftCrtl = GLFW_KEY_LEFT_CONTROL,
		LeftAlt = GLFW_KEY_LEFT_ALT
	};

#pragma region MouseMovement

	static int GetMouseX() { return GetInstance().m_mouseX; };
	static int GetMouseY() { return GetInstance().m_mouseY; };
	static float GetMouseDeltaX() { return (float)GetInstance().m_deltaX / GetInstance().m_width; };
	static float GetMouseDeltaY() { return (float)GetInstance().m_deltaY / GetInstance().m_height; };

	static void SetMousePosition(int x, int y) {

		auto& instance = GetInstance();
		instance.m_deltaX += x - instance.m_mouseX;
		instance.m_deltaY += y - instance.m_mouseY;

		instance.m_mouseX = x;
		instance.m_mouseY = y;

		glfwSetCursorPos(instance.m_window, x, y);

	}

	// Hides mouse cursor
	static void ShowMouse() { glfwSetInputMode(GetInstance().m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }

	// Hides mouse cursor
	static void HideMouse() { glfwSetInputMode(GetInstance().m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN); }

#pragma endregion

#pragma region MouseButtons

	static bool GetMouseButton(MouseButton button)
	{
		return GetInstance().m_mouseKeysTable[(int)button] >= IS_PRESSED;
	}

	static bool GetMouseButtonDown(MouseButton button)
	{
		return GetInstance().m_mouseKeysTable[(int)button] == FIRST_PRESSED;
	}

	static bool GetMouseButtonUp(MouseButton button)
	{
		return GetInstance().m_mouseKeysTable[(int)button] == RELEASED_THIS_FRAME;
	}

#pragma endregion


#pragma region Keyboard

	static bool GetKey(KeyCode key)
	{
		return GetInstance().m_keyTable[key] >= IS_PRESSED;
	}

	static bool GetKeyDown(unsigned char key)
	{
		return GetInstance().m_keyTable[key] == FIRST_PRESSED;
	}

	static bool GetKeyUp(unsigned char key)
	{
		return GetInstance().m_keyTable[key] == RELEASED_THIS_FRAME;
	}



#pragma endregion

private:

	static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
	{
		auto& instance = GetInstance();
		instance.m_mouseKeysTable[(int)MouseButton::WheelUp] = yoffset > 0 ? FIRST_PRESSED : NOT_PRESSED;
		instance.m_mouseKeysTable[(int)MouseButton::WheelDown] = yoffset < 0 ? FIRST_PRESSED : NOT_PRESSED;
	}

	void updateTables(GLFWwindow* window, int width, int height)
	{
		m_width = width;
		m_height = height;

		if (m_window == nullptr) {
			m_window = window;

			// Set the scroll callback
			glfwSetScrollCallback(window, scroll_callback);
		}

		// Fetches the coordinates of the cursor
		double mouseX, mouseY;
		glfwGetCursorPos(window, &mouseX, &mouseY);

		m_deltaX = mouseX - m_mouseX;
		m_deltaY = mouseY - m_mouseY;

		m_mouseX = mouseX;
		m_mouseY = mouseY;

		// GLFW defined buttons transformed to my values
		int b[3] = {
			(int)MouseButton::LeftClick,
			(int)MouseButton::RightClick,
			(int)MouseButton::MiddleClick
		};

		int g[3] = {
			GLFW_MOUSE_BUTTON_LEFT,
			GLFW_MOUSE_BUTTON_RIGHT,
			GLFW_MOUSE_BUTTON_MIDDLE
		};

		for (int i = 0; i < 3; i++) {

			int current = glfwGetMouseButton(m_window, g[i]);
			int previous = m_mouseKeysTable[b[i]];

			if (current == IS_PRESSED && (previous == NOT_PRESSED || previous == RELEASED_THIS_FRAME))
			{
				// If it wasn't pressed in previous frame, it's a first press
				current = FIRST_PRESSED;
				
			}
			else if (current == NOT_PRESSED && (previous == FIRST_PRESSED || previous == IS_PRESSED))
			{
				// If it was pressed in previous frame, it's a first release
				current = RELEASED_THIS_FRAME;
			}

			m_mouseKeysTable[b[i]] = current;
		}


		for (int i = 0; i < SUPPORTED_KEYS; i++) {

			int current = glfwGetKey(m_window, i);
			int previous = m_keyTable[i];

			if (current == IS_PRESSED && (previous == NOT_PRESSED || previous == RELEASED_THIS_FRAME))
			{
				// If it wasn't pressed in previous frame, it's a first press
				current = FIRST_PRESSED;
			}
			else if (current == NOT_PRESSED && (previous == FIRST_PRESSED || previous == IS_PRESSED))
			{
				// If it was pressed in previous frame, it's a first release
				current = RELEASED_THIS_FRAME;
			}

			m_keyTable[i] = current;
		}
	}

	void clearTables() {
		m_mouseKeysTable[(int)MouseButton::WheelUp] = NOT_PRESSED;
		m_mouseKeysTable[(int)MouseButton::WheelDown] = NOT_PRESSED;
	}

public:

	static void update(GLFWwindow* window, int width, int height) {
		auto& instance = GetInstance();
		instance.updateTables(window, width, height);
	}

	/// <summary>
	/// Must be called after update but before render
	/// </summary>
	static void clear() {
		auto& instance = GetInstance();
		instance.clearTables();
	}

};

