#pragma once
class Color
{
private:
	float m_red;
	float m_green;
	float m_blue;

public:

	Color(float r, float g, float b) : m_red(r), m_green(g), m_blue(b) {}

	inline float GetRedComponent() const { return this->m_red; }
	inline float GetGreenComponent() const { return this->m_green; }
	inline float GetBlueComponent() const { return this->m_blue; }

	inline void SetRedComponent(const float& redComponentToSet) { this->m_red = ((redComponentToSet >= 0 && redComponentToSet <= 1)? redComponentToSet : this->m_red);}
	inline void SetGreenComponent(const float& greenComponentToSet) { this->m_green = ((greenComponentToSet >= 0 && greenComponentToSet <= 1) ? greenComponentToSet : this->m_green);}
	inline void SetBlueComponent(const float& blueComponentToSet) { this->m_blue = ((blueComponentToSet >= 0 && blueComponentToSet <= 1) ? blueComponentToSet : this->m_blue);}

};

