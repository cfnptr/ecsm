# ECSM

Easy to use template based C++ **Entity Component System Manager** library.

The ECS pattern, or Entity-Component-System pattern, is a design pattern commonly used in game development and 
simulation software. It is a way to organize and manage the behavior and data of objects within a system. 
The ECS pattern is particularly useful for systems with a large number of entities that can have varying and 
dynamic sets of attributes.

See the [documentation](https://cfnptr.github.io/ecsm)

## Features

* Straightforward template architecture
* Custom event creation support
* Cache friendly linear pools
* Acceptable compilation time
* Fast component iteration
* Fast entity component access

## Usage example

```cpp
using namespace ecsm;

struct RigidBodyComponent final : public Component
{
    float size = 0.0f;
};

class PhysicsSystem final : public System
{
    LinearPool<RigidBodyComponent, false> components;

    PhysicsSystem(Manager* manager) : System(manager)
    {
        SUBSCRIBE_TO_EVENT("Update", PhysicsSystem::update);
    }
    ~PhysicsSystem() final
    {
		auto manager = getManager();
		if (manager->isRunning())
        	UNSUBSCRIBE_FROM_EVENT("Update", PhysicsSystem::update);
    }

    void update()
    {
        // Process components...
    }

    const string& getComponentName() const final
	{
		static const string name = "Rigid Body";
		return name;
	}
    type_index getComponentType() const final
	{
		return typeid(RigidBodyComponent);
	}
    ID<Component> createComponent(ID<Entity> entity) final
	{
		return ID<Component>(components.create());
	}
    void destroyComponent(ID<Component> instance) final
	{
		components.destroy(ID<RigidBodyComponent>(instance));
	}
    View<Component> getComponent(ID<Component> instance) final
    {
		return View<Component>(components.get(ID<RigidBodyComponent>(instance)));
	}
    void disposeComponents() final { components.dispose(); }

    friend class ecsm::Manager;
};

void ecsmExample()
{
    ecsm::Manager manager;

    manager.createSystem<PhysicsSystem>();
    manager.createSystem<GraphicsSystem>(false, 123);
    // ...

    manager.initialize();

    auto rigidBody = manager.createEntity();
    auto rigidBodyComponent = manager.add<RigidBodyComponent>(rigidBody);
    rigidBodyComponent->size = 1.0f;

    manager.start();
}
```

Use ```#define ECSM_DEEP_ID_TRACKING``` to detect and debug pool memory corruptions and errors.

## Supported operating systems

* Windows
* macOS
* Ubuntu (Linux)

## Build requirements

* C++17 compiler
* [Git 2.30+](https://git-scm.com/)
* [CMake 3.16+](https://cmake.org/)

Use building [instructions](BUILDING.md) to install all required tools and libraries.

### CMake options

| Name              | Description               | Default value |
|-------------------|---------------------------|---------------|
| ECSM_BUILD_SHARED | Build ECSM shared library | `ON`          |
| ECSM_BUILD_TESTS  | Build ECSM library tests  | `ON`          |

### CMake targets

| Name        | Description          | Windows | macOS    | Linux |
|-------------|----------------------|---------|----------|-------|
| ecsm-static | Static ECSM library  | `.lib`  | `.a`     | `.a`  |
| ecsm-shared | Dynamic ECSM library | `.dll`  | `.dylib` | `.so` |

## Cloning ![CI](https://github.com/cfnptr/ecsm/actions/workflows/cmake.yml/badge.svg)

```
git clone https://github.com/cfnptr/ecsm
```