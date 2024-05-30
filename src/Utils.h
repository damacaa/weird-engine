#ifndef ExtraUtils
#define ExtraUtils

#include <sstream>
#include <iomanip>
#include <iostream>



static unsigned int hash(unsigned int x) {
	x = ((x >> 16) ^ x) * 0x119de1f3;
	x = ((x >> 16) ^ x) * 0x119de1f3;
	x = (x >> 16) ^ x;
	return x;
}



#endif