#pragma once
#include <vector>
#include <string>
#include "ComponentManager.h"

class Component;
class Transform;
class Collider;
class Entity
{
private:
	Transform* m_transform;
	std::vector<Component*> m_components;
	std::vector<Component*>& GetAllComponents() { return m_components; }
public:
	std::string name;
	 
	Entity(std::string name = "Entity");
	~Entity();

	Transform& GetTransform() { return *m_transform; };

	template<class T>
	T* AddComponent();

	template<class T>
	T* GetComponent();
	template<class T>
	std::vector<T*> GetComponents();

	void OnCollisionEnter(Collider& collider);
	void OnCollisionExit(Collider& collider);
};

template<class T>
inline T* Entity::AddComponent()
{
	T& c = ComponentManager<T>::GetComponent();
	c.SetUp(this);
	m_components.push_back(&c);
	return &c;
}

// https://stackoverflow.com/questions/44105058/implementing-component-system-from-unity-in-c
template<class T>
inline T* Entity::GetComponent()
{
	for (Component* c : m_components)
		if (dynamic_cast<T*>(c))
			return (T*)c;

	return nullptr;
}

template<class T>
inline std::vector<T*> Entity::GetComponents()
{
	std::vector<T*> v;
	for (Component* c : m_components)
		if (dynamic_cast<T*>(c))
			v.push_back((T*)c);
	return v;
}


