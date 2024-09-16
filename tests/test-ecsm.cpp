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

class TestSystem final : public ComponentSystem<TestComponent>
{
	TestSystem()
	{
		ECSM_SUBSCRIBE_TO_EVENT("Init", TestSystem::init);
		ECSM_SUBSCRIBE_TO_EVENT("Update", TestSystem::update);
		ECSM_SUBSCRIBE_TO_EVENT("PostUpdate", TestSystem::postUpdate);
	}
	~TestSystem() final
	{
		if (Manager::Instance::get()->isRunning())
		{
			ECSM_UNSUBSCRIBE_FROM_EVENT("Init", TestSystem::init);
			ECSM_UNSUBSCRIBE_FROM_EVENT("Update", TestSystem::update);
			ECSM_UNSUBSCRIBE_FROM_EVENT("PostUpdate", TestSystem::postUpdate);
		}
	}

	void copyComponent(View<Component> source, View<Component> destination) final
	{
		const auto sourceView = View<TestComponent>(source);
		auto destinationView = View<TestComponent>(destination);
		destinationView->ID = sourceView->ID;
		destinationView->someData = sourceView->someData;
	}
	const string& getComponentName() const final
	{
		static const string name = "Test";
		return name;
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
			auto componentView = &componentData[i];
			if (componentView->ID == 0) // Skip unallocated components
				continue;

			componentView->ID++;
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
};

//**********************************************************************************************************************
static void testCommonFlow()
{
	auto manager = new Manager();
	auto managerSingleton = Manager::Instance::get();

	if (manager != managerSingleton)
		throw runtime_error("Different manager and singleton instance.");

	manager->registerEventAfter("PostUpdate", "Update");

	if (manager->has<TestSystem>())
		throw runtime_error("Test system is not yet created.");

	manager->createSystem<TestSystem>();

	if (!manager->has<TestSystem>())
		throw runtime_error("No created test system found.");

	auto system = manager->get<TestSystem>();

	if (system->isInitialized != false)
		throw runtime_error("Test system is already initialized.");

	manager->initialize();

	if (system->isInitialized != true)
		throw runtime_error("Test system is not initialized.");

	auto baseSystem = dynamic_cast<System*>(system);

	if (baseSystem->getComponentName() != "Test")
		throw runtime_error("Bad test system component name.");
	if (baseSystem->getComponentType() != typeid(TestComponent))
		throw runtime_error("Bad test system component type.");

	auto testEntity = manager->createEntity();

	if (manager->has<TestComponent>(testEntity))
		throw runtime_error("Test component is not yet created.");

	auto testView = manager->add<TestComponent>(testEntity);
	testView->ID = 1;
	testView->someData = 123.456f;

	if (!manager->has<TestComponent>(testEntity))
		throw runtime_error("No created test component found.");
	if (testView->getEntity() != testEntity)
		throw runtime_error("Bad test component entity instance.");

	testView = manager->get<TestComponent>(testEntity);

	if (testView->ID != 1 || testView->someData != 123.456f)
		throw runtime_error("Bad test component data before update.");

	if (system->updateCounter != 0 || system->postUpdateCounter != 0)
		throw runtime_error("Bad test system data before update.");

	manager->update();

	if (system->updateCounter != 1 || system->postUpdateCounter != 2)
		throw runtime_error("Bad test system data after update.");

	testView = manager->get<TestComponent>(testEntity);

	if (testView->ID != 2)
		throw runtime_error("Bad test component data after update.");
	if (testView->getEntity() != testEntity)
		throw runtime_error("Bad test component entity instance after update.");

	manager->remove<TestComponent>(testEntity);

	if (manager->has<TestComponent>(testEntity))
		throw runtime_error("Test component is not destroyed.");

	// Note: after destruction component is still accessible until dispose call.
	testView = manager->get<TestComponent>(testEntity);
	if (testView->ID != 2)
		throw runtime_error("Bad test component data after destroy.");

	auto componentMemory = *testView;
	manager->disposeGarbageComponents();
	manager->disposeSystemComponents();
	manager->disposeEntities();

	if (componentMemory->ID != 0) // WARNING! You should't do this, it's just safety check!
		throw runtime_error("Bad test component data after dispose.");

	manager->destroySystem<TestSystem>();

	if (manager->has<TestSystem>())
		throw runtime_error("Test system is not destroyed.");

	manager->unregisterEvent("PostUpdate");
	delete manager;
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
		auto testView = manager->add<TestComponent>(entity);
		testView->ID = i;
		testView->someData = rand();

		if (i == 2)
			thirdEntity = entity;
	}

	auto testView = manager->get<TestComponent>(thirdEntity);

	if (testView->ID != 2)
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
	auto firstTestView = manager->add<TestComponent>(firstEntity);
	firstTestView->ID = 12345;
	auto secondTestView = manager->add<TestComponent>(secondEntity);
	secondTestView->ID = 54321;

	manager->copy<TestComponent>(firstEntity, secondEntity);

	secondTestView = manager->get<TestComponent>(secondEntity);
	if (secondTestView->ID != 12345)
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
	auto componentView = manager->add<TestComponent>(entity);
	componentView->someData = 13.37f;
	componentView->counter = &stackCounter;

	manager->remove<TestComponent>(entity);

	componentView = manager->get<TestComponent>(entity);
	if (componentView->someData != 13.37f)
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