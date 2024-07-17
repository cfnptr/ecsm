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
	int* counter = nullptr;

	bool destroy()
	{
		if (counter)
			*counter = *counter - 1;
		return true;
	}
};

class TestSystem final : public System
{
	LinearPool<TestComponent> components;

	TestSystem()
	{
		auto manager = Manager::getInstance();
		SUBSCRIBE_TO_EVENT("Init", TestSystem::init);
		SUBSCRIBE_TO_EVENT("Update", TestSystem::update);
		SUBSCRIBE_TO_EVENT("PostUpdate", TestSystem::postUpdate);
	}
	~TestSystem() final
	{
		auto manager = Manager::getInstance();
		if (manager->isRunning())
		{
			UNSUBSCRIBE_FROM_EVENT("Init", TestSystem::init);
			UNSUBSCRIBE_FROM_EVENT("Update", TestSystem::update);
			UNSUBSCRIBE_FROM_EVENT("PostUpdate", TestSystem::postUpdate);
		}
	}

	ID<Component> createComponent(ID<Entity> entity) final
	{
		return ID<Component>(components.create());
	}
	void destroyComponent(ID<Component> instance) final
	{
		components.destroy(ID<TestComponent>(instance));
	}
	void copyComponent(View<Component> source, View<Component> destination) final
	{
		const auto sourceView = View<TestComponent>(source);
		auto destinationView = View<TestComponent>(destination);
		destinationView->ID = sourceView->ID;
		destinationView->someData = sourceView->someData;
	}

	void init()
	{
		isInitialized = true;
	}
	void update()
	{
		updateCounter++;

		auto componentData = components.getData();
		auto occupancy = components.getOccupancy();

		for (uint32_t i = 0; i < occupancy; i++)
		{
			auto& component = componentData[i];
			if (component.ID == 0) // Skip unallocated components
				continue;

			component.ID++;
		}
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

	const string& getComponentName() const final
	{
		static const string name = "Test";
		return name;
	}
	type_index getComponentType() const final
	{
		return typeid(TestComponent);
	}
	View<Component> getComponent(ID<Component> instance) final
	{
		return View<Component>(components.get(ID<TestComponent>(instance)));
	}
	void disposeComponents() final
	{
		components.dispose();
	}
};

//**********************************************************************************************************************
static void testCommonFlow()
{
	Manager manager;

	manager.registerEventAfter("PostUpdate", "Update");

	if (manager.has<TestSystem>())
		throw runtime_error("Test system is not yet created.");

	manager.createSystem<TestSystem>();

	if (!manager.has<TestSystem>())
		throw runtime_error("No created test system found.");

	auto system = manager.get<TestSystem>();

	if (system->isInitialized != false)
		throw runtime_error("Test system is already initialized.");

	manager.initialize();

	if (system->isInitialized != true)
		throw runtime_error("Test system is not initialized.");

	auto baseSystem = dynamic_cast<System*>(system);

	if (baseSystem->getComponentName() != "Test")
		throw runtime_error("Bad test system component name.");
	if (baseSystem->getComponentType() != typeid(TestComponent))
		throw runtime_error("Bad test system component type.");

	auto testEntity = manager.createEntity();

	if (manager.has<TestComponent>(testEntity))
		throw runtime_error("Test component is not yet created.");

	auto testComponent = manager.add<TestComponent>(testEntity);
	testComponent->ID = 1;
	testComponent->someData = 123.456f;

	if (!manager.has<TestComponent>(testEntity))
		throw runtime_error("No created test component found.");
	if (testComponent->getEntity() != testEntity)
		throw runtime_error("Bad test component entity instance.");

	testComponent = manager.get<TestComponent>(testEntity);

	if (testComponent->ID != 1 || testComponent->someData != 123.456f)
		throw runtime_error("Bad test component data before update.");

	if (system->updateCounter != 0 || system->postUpdateCounter != 0)
		throw runtime_error("Bad test system data before update.");

	manager.update();

	if (system->updateCounter != 1 || system->postUpdateCounter != 2)
		throw runtime_error("Bad test system data after update.");

	testComponent = manager.get<TestComponent>(testEntity);

	if (testComponent->ID != 2)
		throw runtime_error("Bad test component data after update.");
	if (testComponent->getEntity() != testEntity)
		throw runtime_error("Bad test component entity instance after update.");

	manager.remove<TestComponent>(testEntity);

	if (manager.has<TestComponent>(testEntity))
		throw runtime_error("Test component is not destroyed.");

	// Note: after destruction component is still accessible until dispose call.
	testComponent = manager.get<TestComponent>(testEntity);
	if (testComponent->ID != 2)
		throw runtime_error("Bad test component data after destroy.");

	auto componentMemory = *testComponent;
	manager.disposeGarbageComponents();
	manager.disposeSystemComponents();
	manager.disposeEntities();

	if (componentMemory->ID != 0) // WARNING! You should't do this, it's just safety check!
		throw runtime_error("Bad test component data after dispose.");

	manager.destroySystem<TestSystem>();

	if (manager.has<TestSystem>())
		throw runtime_error("Test system is not destroyed.");

	manager.unregisterEvent("PostUpdate");
}

//**********************************************************************************************************************
static void testEntityAllocation()
{
	auto manager = new Manager();
	manager->registerEventAfter("PostUpdate", "Update");
	manager->createSystem<TestSystem>();

	const uint32_t entityCount = 123;
	ID<Entity> thirdEntity = {};

	for (uint32_t i = 0; i < entityCount; i++)
	{
		auto entity = manager->createEntity();
		auto testComponent = manager->add<TestComponent>(entity);
		testComponent->ID = i;
		testComponent->someData = rand();

		if (i == 2)
			thirdEntity = entity;
	}

	auto testComponent = manager->get<TestComponent>(thirdEntity);

	if (testComponent->ID != 2)
		throw runtime_error("Bad test component ID.");

	manager->destroy(thirdEntity);
	delete manager;
}

//**********************************************************************************************************************
static void testComponentCopy()
{
	auto manager = new Manager();
	manager->registerEventAfter("PostUpdate", "Update");
	manager->createSystem<TestSystem>();

	auto firstEntity = manager->createEntity();
	auto secondEntity = manager->createEntity();
	auto firstComponent = manager->add<TestComponent>(firstEntity);
	firstComponent->ID = 12345;
	auto secondComponent = manager->add<TestComponent>(secondEntity);
	secondComponent->ID = 54321;

	manager->copy<TestComponent>(firstEntity, secondEntity);

	secondComponent = manager->get<TestComponent>(secondEntity);
	if (secondComponent->ID != 12345)
		throw runtime_error("Bad second test component ID.");

	delete manager;
}

//**********************************************************************************************************************
static void testDisposeFlow()
{
	auto manager = new Manager();
	manager->registerEventAfter("PostUpdate", "Update");
	manager->createSystem<TestSystem>();

	int stackCounter = 1;
	auto entity = manager->createEntity();
	auto component = manager->add<TestComponent>(entity);
	component->someData = 13.37f;
	component->counter = &stackCounter;

	manager->remove<TestComponent>(entity);

	component = manager->get<TestComponent>(entity);
	if (component->someData != 13.37f)
		throw runtime_error("Bad test component ID after remove.");
	if (stackCounter != 1)
		throw runtime_error("Bad stack counter after component remove.");

	manager->disposeGarbageComponents();
	manager->disposeSystemComponents();

	if (stackCounter != 0)
		throw runtime_error("Bad stack counter after component dispose.");

	delete manager;
}

//**********************************************************************************************************************
int main()
{
	testCommonFlow();
	testEntityAllocation();
	testComponentCopy();
	testDisposeFlow();
	// TODO: test Ref<> and other manager functions
}