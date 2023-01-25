#pragma once
#include <vector>
#include <map>
#include <string>

#include <mutex>

class DebugHelper
{
private:
	std::map<std::string, std::chrono::steady_clock::time_point> timePointMap;

	std::chrono::steady_clock::time_point lastTimepoint;
	std::string lastName;
	std::map<std::string, double> durations;
	std::map<std::string, int> occurances;

	std::string path = "C:/Debug/";

public:
	DebugHelper() {};
	~DebugHelper() {};
	bool enabled = true;

	void RecordTime(std::string name);
	void Wait();
	void PrintTimes(std::string fileName = "times", std::string description = "");

	void PrintValue(std::string value, std::string fileName = "value");
};

