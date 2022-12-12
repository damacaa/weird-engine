#pragma once
class Color
{
private:
	float redComponent;
	float greenComponent;
	float blueComponent;

public:

	Color(float r, float g, float b) : redComponent(r), greenComponent(g), blueComponent(b) {}

	inline float GetRedComponent() const { return this->redComponent; }
	inline float GetGreenComponent() const { return this->greenComponent; }
	inline float GetBlueComponent() const { return this->blueComponent; }

	inline void SetRedComponent(const float& redComponentToSet) { this->redComponent = ((redComponentToSet >= 0 && redComponentToSet <= 1)? redComponentToSet : this->redComponent);}
	inline void SetGreenComponent(const float& greenComponentToSet) { this->greenComponent = ((greenComponentToSet >= 0 && greenComponentToSet <= 1) ? greenComponentToSet : this->greenComponent);}
	inline void SetBlueComponent(const float& blueComponentToSet) { this->blueComponent = ((blueComponentToSet >= 0 && blueComponentToSet <= 1) ? blueComponentToSet : this->blueComponent);}

};

