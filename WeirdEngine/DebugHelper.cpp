#include "DebugHelper.h"
#include <iostream>
#include <fstream>  
#include <chrono>

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

void DebugHelper::PrintValue(std::string value, std::string fileName)
{
	std::ofstream outfile(path+ fileName + ".txt");

	outfile << value << std::endl;

	outfile.close();
}

void DebugHelper::RecordTime(std::string name)
{
	if (!enabled)
		return;

	std::chrono::steady_clock::time_point now = high_resolution_clock::now();

	duration<double, std::milli> ms_double = now - lastTimepoint;
	double duration = ms_double.count();

	if (lastName.empty()) {
		lastName = name;
		lastTimepoint = now;
		return;
	}

	std::map<std::string, double>::iterator it = durations.find(lastName);

	if (it != durations.end())
	{
		//First time
		occurances[lastName] = 1;
		durations[lastName] = duration;
	}
	else {
		occurances[lastName] += 1;
		durations[lastName] += duration;
	}

	lastName = name;
	lastTimepoint = now;
}

void DebugHelper::Wait()
{
	RecordTime("");
}

void DebugHelper::PrintTimes(std::string fileName, std::string description)
{
	if (!enabled)
		return;

	//std::ofstream outfile(fileName + ".txt");
	std::ofstream outfile(path+ fileName + ".txt", std::ofstream::app | std::ofstream::out);
	outfile.precision(3);

	if (!outfile.is_open())
	{
		std::cout << "Error opening file";
		return;
	}

	if (!description.empty())
		outfile << description << std::endl;

	int maxLength = 0;
	double total = 0;
	std::map<std::string, double>::iterator it;
	for (it = durations.begin(); it != durations.end(); ++it) {
		if (it->first.empty())
			continue;

		maxLength = max(maxLength, it->first.size());
		total += durations[it->first] / (double)occurances[it->first];
	}
	maxLength += 10;

	for (it = durations.begin(); it != durations.end(); ++it) {
		if (it->first.empty())
			continue;

		double d = durations[it->first] / (double)occurances[it->first];
		outfile << it->first << ": ";
		for (size_t i = it->first.size(); i < maxLength; i++)
		{
			outfile << " ";
		}
		outfile << d << "ms --> ";
		outfile << 100.0 * d / total << "%" << std::endl;
	}

	outfile << "Total: " << total << "ms" << std::endl << std::endl;
	outfile.close();

	std::cout << total << "ms" << std::endl;
}