#include "Input.h"
int Input::_mouseX = 0;
int Input::_mouseY = 0;

bool Input::_mouseHasBeenMoved;
int Input::_deltaX = 0;
int Input::_deltaY = 0;

int Input::_keyTable[256]{ 0 };
int Input::_mouseKeysTable[5];