#pragma once

#include <vector>
using namespace std;

template<class C> class ComponentManager
{
private:
	C* m_components;

	size_t m_initialPoolSize = 305;
	size_t m_usedComponents = 0;

	ComponentManager() {
		m_components = new C[m_initialPoolSize];
	};

	~ComponentManager();

	static ComponentManager& GetInstance() {
		static ComponentManager* instance = new ComponentManager();
		return *instance;
	};

public:
	
	static C& GetComponent();

	static void Render();
	static void Update();
	static void FixedUpdate();
	static void LateUpdate();

	static C* GetActiveComponents(int* count);
};

template<class C>
inline ComponentManager<C>::~ComponentManager()
{
	delete m_components;
}

template<class C>
inline C& ComponentManager<C>::GetComponent()
{
	auto& instance = GetInstance();
	++instance.m_usedComponents;
	return instance.m_components[instance.m_usedComponents-1];
}

template<class C>
inline void ComponentManager<C>::Render()
{
	auto& manager = GetInstance();
	for (int i = 0; i < manager.m_usedComponents; i++)
	{
		manager.m_components[i].Render();
	}
}

template<class C>
inline void ComponentManager<C>::Update()
{
	auto& manager = GetInstance();
	for (int i = 0; i < manager.m_usedComponents; i++)
	{
		manager.m_components[i].Update();
	}
}

template<class C>
inline C* ComponentManager<C>::GetActiveComponents(int* count)
{
	*count = GetInstance().m_usedComponents;
	return GetInstance().m_components;
}
