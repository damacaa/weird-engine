#pragma once

#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

static unsigned int hash(unsigned int x) {
	x = ((x >> 16) ^ x) * 0x119de1f3;
	x = ((x >> 16) ^ x) * 0x119de1f3;
	x = (x >> 16) ^ x;
	return x;
}

static std::string get_file_contents(const char* filename)
{
	std::ifstream in(filename, std::ios::binary);
	if (in)
	{
		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		return(contents);
	}
	throw(errno);
}

static void saveToFile(const char* filename, std::string content)
{
	// Open the file in output mode, creating it if it doesn't exist
	std::ofstream outFile(filename, std::ios::out | std::ios::trunc);

	// Check if the file was opened successfully
	if (outFile.is_open())
	{
		// Write the content to the file
		outFile << content;

		// Close the file
		outFile.close();
	}
	else
	{
		// Handle the error if the file couldn't be opened
		std::cerr << "Error: Could not open file " << filename << " for writing.\n";
	}
}

static bool checkIfFileExists(const char* filename)
{
	// Try opening the file
	std::ifstream file(filename);

	// Return true if the file can be opened, false otherwise
	return file.good();
}
