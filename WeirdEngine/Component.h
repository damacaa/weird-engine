#pragma once
#include "Entity.h"
class Component
{
protected:
	static std::vector<Component*> _instances;

	Entity* _entity;

	Component(Entity* owner) {
		_entity = owner;
		_instances.push_back(this);
	};


public:

	Entity* GetEntity() { return _entity; };

	virtual void Awake() {}
	virtual void Start() {}
	virtual void Render() {}
	virtual void Update() {}
	virtual void FixedUpdate() {}
	virtual void LateUpdate() {}

	const static std::vector<Component*> Instances() { return _instances; }
};

