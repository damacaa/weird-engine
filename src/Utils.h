#ifndef ExtraUtils
#define ExtraUtils

#include <sstream>
#include <iomanip>
#include <iostream>
#include <glm/mat4x4.hpp>


static unsigned int hash(unsigned int x) {
	x = ((x >> 16) ^ x) * 0x119de1f3;
	x = ((x >> 16) ^ x) * 0x119de1f3;
	x = (x >> 16) ^ x;
	return x;
}

class ExtraUtils {

public:

	
};

#endif