#pragma once
#include "Entity.h"
class Component
{
protected:
	static std::vector<Component*> m_instances;

	Entity* m_entity;

	Component(Entity* owner) {
		m_entity = owner;
		m_instances.push_back(this);
		Awake();
	};


public:

	Entity& GetEntity() { return *m_entity; };

	virtual void Awake() {}
	virtual void Start() {}
	virtual void Render() {}
	virtual void Update() {}
	virtual void FixedUpdate() {}
	virtual void LateUpdate() {}

	const static std::vector<Component*> Instances() { return m_instances; }
};

