#pragma once
#include "Solid.h"
#include <string>
#include <GL/glut.h>
using namespace std;
class Text : public Solid
{
private: 
	string text;
public:
	Text() : Solid(), text(" ") {};

	inline string GetText() const { return this->text; }

	inline void SetText(const string& textToSet) { this->text = textToSet; }

	void Render();
};

