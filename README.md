# ECSM

Easy to use template based C++ **Entity Component System Manager** [library](https://github.com/cfnptr/ecsm).

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
* Singleton class pattern

## Usage example

```cpp
using namespace ecsm;

struct RigidBodyComponent final : public Component
{
    float size = 0.0f;
};

class PhysicsSystem final : public ComponentSystem<RigidBodyComponent, false>
{
    PhysicsSystem()
    {
        auto manager = Manager::Instance::get();
        ECSM_SUBSCRIBE_TO_EVENT("Update", PhysicsSystem::update);
    }
    ~PhysicsSystem() final
    {
        if (Manager::get()->isRunning)
        {
            auto manager = Manager::Instance::get();
            ECSM_UNSUBSCRIBE_FROM_EVENT("Update", PhysicsSystem::update);
        }
    }

    void update()
    {
        for (auto& component : components)
        {
            if (!component.getEntity())
                continue;

            // Process your component
        }
    }

    friend class ecsm::Manager;
};

void ecsmExample()
{
    auto manager = new ecsm::Manager();
    manager->createSystem<PhysicsSystem>();
    manager->createSystem<GraphicsSystem>(false, 123); // System arguments
    // ...

    manager->initialize();

    auto rigidBody = manager->createEntity();
    auto rigidBodyView = manager->add<RigidBodyComponent>(rigidBody);
    rigidBodyView->size = 1.0f;

    manager->start();

    delete manager;
}
```

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

## Cloning

```
git clone --recursive https://github.com/cfnptr/ecsm
```

## Building ![CI](https://github.com/cfnptr/ecsm/actions/workflows/cmake.yml/badge.svg)

* Windows: ```./scripts/build-release.bat```
* macOS / Ubuntu: ```./scripts/build-release.sh```

## Third-party

* [robin-map](https://github.com/Tessil/robin-map) (MIT license)

### Special thanks to Sahak Grigoryan.
