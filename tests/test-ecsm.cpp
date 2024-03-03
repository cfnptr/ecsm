// Copyright 2022-2024 Nikita Fediuchin. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ecsm.hpp"
using namespace ecsm;

struct TestComponent final : public Component
{
	int ID = 0;
	float someData = 0.0f;
};

class TestSystem final : public System
{
	LinearPool<TestComponent, false> components;

	TestSystem(Manager* manager) : System(manager)
	{
		SUBSCRIBE_TO_EVENT("Init", TestSystem::init);
		SUBSCRIBE_TO_EVENT("Update", TestSystem::update);
		SUBSCRIBE_TO_EVENT("PostUpdate", TestSystem::postUpdate);
	}
	~TestSystem() final
	{
		auto manager = getManager();
		if (manager->isRunning())
		{
			UNSUBSCRIBE_FROM_EVENT("Init", TestSystem::init);
			UNSUBSCRIBE_FROM_EVENT("Update", TestSystem::update);
			UNSUBSCRIBE_FROM_EVENT("PostUpdate", TestSystem::postUpdate);
		}
	}

	string_view getComponentName() const final { return "Test"; }
	type_index getComponentType() const final { return typeid(TestComponent); }
	ID<Component> createComponent(ID<Entity> entity) final { return ID<Component>(components.create()); }
	void destroyComponent(ID<Component> instance) final { components.destroy(ID<TestComponent>(instance)); }
	View<Component> getComponent(ID<Component> instance) final {
		return View<Component>(components.get(ID<TestComponent>(instance))); }
	void disposeComponents() final { components.dispose(); }

	void init()
	{
		isInitialized = true;
	}
	void update()
	{
		updateCounter++;
	}
	void postUpdate()
	{
		postUpdateCounter = 2;
	}

	friend class ecsm::Manager;
public:
	int updateCounter = 0;
	int postUpdateCounter = 0;
	bool isInitialized = false;
};

//**********************************************************************************************************************
int main()
{
	Manager manager;

	manager.registerEventAfter("PostUpdate", "Update");

	if (manager.has<TestSystem>())
		throw runtime_error("Found bad test system.");

	manager.createSystem<TestSystem>();

	if (!manager.has<TestSystem>())
		throw runtime_error("No added test system.");
	
	auto system = manager.get<TestSystem>();

	if (system->isInitialized != false)
		throw runtime_error("Test system is initialized.");

	manager.initialize();
	
	if (system->isInitialized != true)
		throw runtime_error("Test system is not initialized.");

	if (system->getComponentName() != "Test")
		throw runtime_error("Bad system component name.");

	auto testEntity = manager.createEntity();

	if (manager.has<TestComponent>(testEntity))
		throw runtime_error("Found bad test component.");

	auto testComponent = manager.add<TestComponent>(testEntity);
	testComponent->ID = 1;
	testComponent->someData = 123.456f;

	if (!manager.has<TestComponent>(testEntity))
		throw runtime_error("No added test component.");

	testComponent = manager.get<TestComponent>(testEntity);

	if (testComponent->ID != 1 || testComponent->someData != 123.456f)
		throw runtime_error("Bad test component data.");

	if (system->updateCounter != 0 || system->postUpdateCounter != 0)
		throw runtime_error("Bad test system data.");

	manager.update();

	if (system->updateCounter != 1 || system->postUpdateCounter != 2)
		throw runtime_error("Bad test system data.");

	manager.remove<TestComponent>(testEntity);

	if (manager.has<TestComponent>(testEntity))
		throw runtime_error("Found bad test component.");

	manager.unregisterEvent("PostUpdate");
}