#pragma once
#include <vector>
#include <string>
class Component;
class Transform;
class Entity
{
private:
	std::vector<Component*> m_components;
	Transform* m_transform;
public:
	std::string name;

	Entity(std::string name = "Entity");
	~Entity();

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

	Transform& GetTransform() { return *m_transform; };
	std::vector<Component*>& GetComponents() { return m_components; }
};

template<class T>
inline T* Entity::AddComponent()
{
	auto c = new T(this);
	m_components.push_back(c);
	return c;
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


