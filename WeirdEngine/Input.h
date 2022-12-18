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
	static int _mouseX;
	static int _mouseY;
	static bool _mouseHasBeenMoved;
	static int _deltaX;
	static int _deltaY;

	static int _keyTable[256];
	static int _mouseKeysTable[5];

public:

	enum class MouseButton {
		LeftClick = 0,
		MiddleClick = 1,
		RightClick = 2,
		WheelUp = 3,
		WheelDown = 4
	};

	static int GetMouseX() { return _mouseX; };
	static int GetMouseY() { return _mouseY; };
	static float GetMouseDeltaX() { return (float)_deltaX / (float)GLUT_SCREEN_WIDTH; };
	static float GetMouseDeltaY() { return (float)_deltaY / (float)GLUT_SCREEN_HEIGHT; };

	static void SetMouseXY(int x, int y) {
		_deltaX = x - _mouseX;
		_mouseX = x;

		_deltaY = y - _mouseY;
		_mouseY = y;

		_mouseHasBeenMoved = true;
	}

	static bool GetMouseButton(MouseButton button) { return _mouseKeysTable[(int)button] > 0; }
	static bool GetMouseButtonDown(MouseButton button) { return _mouseKeysTable[(int)button] == 1; }
	static bool GetMouseButtonUp(MouseButton button) { return _mouseKeysTable[(int)button] == -1; }

	static void HandleMouseButton(int button, int state) { _mouseKeysTable[button] = -((2 * state) - 1); }

	static bool GetKey(unsigned char key) { return _keyTable[toupper(key)] > 0; }
	static bool GetKeyDown(unsigned char key) { return _keyTable[toupper(key)] == 1; }
	static bool GetKeyUp(unsigned char key) { return _keyTable[toupper(key)] == -1; }

	static void PressKey(unsigned char key) {
		_keyTable[toupper(key)]++;
	}

	static void ReleaseKey(unsigned char key) {
		_keyTable[toupper(key)] = -1;
	}

	static void Update() {
		for (size_t i = 0; i < 256; i++)
		{
			// Updates after first frame where button was down
			_keyTable[i] = _keyTable[i] == 1 ? 2 : _keyTable[i];
			_mouseKeysTable[i] = _mouseKeysTable[i] == 1 ? 2 : _mouseKeysTable[i];

			// Updates after first frame where button was up
			_keyTable[i] = _keyTable[i] == -1 ? 0 : _keyTable[i];
			_mouseKeysTable[i] = _mouseKeysTable[i] == -1 ? 0 : _mouseKeysTable[i];


			if (!_mouseHasBeenMoved) {
				_deltaX = 0;
				_deltaY = 0;
			}

			_mouseHasBeenMoved = false;
		}
	}
};

