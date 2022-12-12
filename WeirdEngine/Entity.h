#pragma once
#include <vector>
#include <string>
class Component;
class Transform;
class Entity
{
private:
	std::vector<Component*> _components;
public:
	std::string name;
	Transform* Transform_;


	Entity();

	template<class T>
	T* AddComponent();

	template<class T>
	T* GetComponent();

	template<class T>
	std::vector<T*> GetComponents();

	void Update();
	void FixedUpdate();
	void LateUpdate() {};

	void Render();
};

template<class T>
inline T* Entity::AddComponent()
{
	auto c = new T(this);
	_components.push_back(c);
	return c;
}

// https://stackoverflow.com/questions/44105058/implementing-component-system-from-unity-in-c
template<class T>
inline T* Entity::GetComponent()
{
	for (Component* c : _components)
		if (dynamic_cast<T*>(c))
			return (T*)c;

	return nullptr;
}

template<class T>
inline std::vector<T*> Entity::GetComponents()
{
	std::vector<T*> v;
	for (Component* c : _components)
		if (dynamic_cast<T*>(c))
			v.push_back((T*)c);
	return v;
}


