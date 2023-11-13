# ECSM ![CI](https://github.com/cfnptr/ecsm/actions/workflows/cmake.yml/badge.svg)

Easy to use template based C++ Entity Component System Manager header only library. (ECS)

## Features

* Straightforward template architecture.
* Subsystem creation support.
* Cache friendly linear pools.
* Acceptable compilation time.
* Fast component iteration.
* Fast entity component access.

## Usage example

```c++
struct RigidBodyComponent final : public Component
{
    float size = 0.0f;
};

class PhysicsSystem final : public System
{
    void initialize() final
    {
        // initialize...
    }
    void terminate() final
    {
        // terminate...
    }

    void update() final
    {
        // on update tick...
    }

    friend class ecsm::Manager;
};

void ecsmExample()
{
    ecsm::Manager manager;

    manager.createSystem<PhysicsSystem>();
    manager.createSystem<GraphicsSystem>(false, 123);

    manager.createSubsystem<GraphicsSystem, SkyGxSystem>();
    manager.createSubsystem<GraphicsSystem, MeshGxSystem>("arg");

    manager.initialize();

    auto rigidBody = manager.createEntity();
    auto rigidBodyComponent = manager.add<RigidBodyComponent>(rigidBody);
    rigidBodyComponent->size = 1.0f;

    manager.start();
}
```

## Supported operating systems

* Ubuntu
* MacOS
* Windows

## Build requirements

* C++17 compiler
* [Git 2.30+](https://git-scm.com/)
* [CMake 3.16+](https://cmake.org/)

### CMake options

| Name             | Description              | Default value |
|------------------|--------------------------|---------------|
| ECSM_BUILD_TESTS | Build ECSM library tests | `ON`          |

## Cloning

```
git clone https://github.com/cfnptr/ecsm
```
