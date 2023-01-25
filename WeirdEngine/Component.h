#pragma once
#include "Entity.h"
class Collider;
class Component
{
protected:

	Entity* m_entity;

public:

	Component():m_entity(nullptr) {};

	virtual void SetUp(Entity* owner) {
		m_entity = owner;
		Awake();
	};

	Entity& GetEntity() { return *m_entity; };

	virtual void Awake() {}
	virtual void Start() {}
	virtual void Render() {}
	virtual void Update() {}
	virtual void FixedUpdate() {}
	virtual void LateUpdate() {}

	virtual void OnCollisionEnter(Collider& collider) {}
	virtual void OnCollisionExit(Collider& collider) {}
};

