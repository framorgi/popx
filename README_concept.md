Architecture Overview
=====================

## Class Hierarchy

```
IEntity <-- IAgent <-- Pop
  ↑
  |
IAgentsManager <-- PopsManager
  |
  └──> owns std::shared_ptr<Pop>
  
IWorld <-- GridWorld
  |
  └──> observes via std::weak_ptr<IEntity>
  
ISimulator <-- Simulator
IRenderer <-- Renderer
```

## Component Descriptions

### Entity System
- **IEntity**: Base interface for all objects that can exist in the world
- **IAgent**: Extends IEntity with AI capabilities (sense, think, act)
- **Pop**: Concrete implementation representing a single individual in the simulation

### World System
- **IWorld**: Interface defining the simulation world/map topology
- **GridWorld**: Grid-based implementation storing entity references in a 2D grid
- **Slice**: Individual grid cell that holds a `std::weak_ptr<IEntity>` to observe entities

### Management System
- **IAgentsManager**: Interface for managing the population of agents
- **PopsManager**: Manages all Pop instances, controls their lifecycle and updates

### Simulation Loop
- **ISimulator** / **Simulator**: Orchestrates the simulation, owns IWorld and IAgentsManager
- **IRenderer** / **Renderer**: Handles visualization, observes IWorld state

## Ownership Model (RESOLVED)

### Clear Single-Direction Ownership

```
┌─────────────────────────────────────────────────────────┐
│                    Main Application                      │
│                                                           │
│  Creates and owns:                                       │
│  • std::shared_ptr<GridWorld>                           │
│  • std::shared_ptr<PopsManager>                         │
│  • std::shared_ptr<Simulator>                           │
│  • std::shared_ptr<Renderer>                            │
└─────────────────────────────────────────────────────────┘
                    │                │
                    ▼                ▼
    ┌───────────────────────┐   ┌──────────────────────┐
    │   PopsManager         │   │    GridWorld         │
    │  (OWNS ENTITIES)      │   │  (OBSERVES ENTITIES) │
    │                       │   │                      │
    │  std::vector<         │   │  Slice stores:       │
    │   std::shared_ptr     │   │   std::weak_ptr      │
    │    <Pop>>             │   │    <IEntity>         │
    │                       │   │                      │
    │  • spawn_population() │   │  • add_entity()      │
    │  • update_cycle()     │   │  • remove_entity()   │
    │  • Controls lifetime  │   │  • is_free()         │
    └───────────────────────┘   └──────────────────────┘
            │                             ▲
            │                             │
            └─────────────────────────────┘
              Passes shared_ptr when
              calling entity->try_spawn()
```

### Key Architectural Decisions

1. **PopsManager is the single source of truth**
   - Owns all Pop instances via `std::shared_ptr<Pop>`
   - Controls entity lifecycle (creation, updates, deletion)
   - Responsible for calling spawn/despawn methods

2. **GridWorld only observes entities**
   - Stores `std::weak_ptr<IEntity>` in Slice objects
   - Can safely check if entities still exist via `.lock()`
   - Automatically handles dangling references when entities are destroyed
   - Does NOT control entity lifetime

3. **Pop stores `std::weak_ptr<IWorld>` as member (EFFICIENT)**
   - Entities store reference to world instead of receiving it as parameter
   - More efficient: avoids repeated parameter passing and atomic ref count operations
   - Still safe: weak_ptr prevents circular dependency
   - Methods like `update()`, `sense()`, `try_move()` use stored world reference
   - Pop locks the weak_ptr when accessing world (safe access pattern)

4. **Pop uses `std::enable_shared_from_this`**
   - Allows safe retrieval of `shared_ptr` to itself
   - `try_spawn()` and `despawn()` use `shared_from_this()`
   - Prevents creating duplicate Pop instances

5. **No circular ownership**
   - PopsManager → owns Pop (shared_ptr)
   - Pop → observes World (weak_ptr)
   - GridWorld → observes Pop (weak_ptr)
   - Clean one-directional dependency flow with safe observation

### Simulation Loop Flow

```
┌─────────────────────────────────────────────────────┐
│  Main Application Loop (PopXApp)                    │
└─────────────────────────────────────────────────────┘
         │
         ├──> Simulator::update()
         │         │
         │         ├──> AgentsManager::update_cycle()
         │         │         │
         │         │         └──> For each Pop:
         │         │                • pop->sense(world)
         │         │                • pop->think()
         │         │                • pop->act()
         │         │
         │         └──> World::update_cycle()
         │
         └──> Renderer::draw()
                   │
                   └──> For each Slice:
                          • weak_ptr.lock()
                          • if valid: draw_entity()
```

### Interface Contracts

**Pop Constructor: Pop(std::weak_ptr<IWorld>)**
- Pop stores weak reference to world at construction time
- More efficient than passing world to every method
- No circular dependency since weak_ptr doesn't increase ref count

**IEntity methods (no world parameter needed):**
- `try_spawn(Position)` - Uses stored world_ member
- `update()` - Uses stored world_ member
- `despawn()` - Uses stored world_ member
- `try_move(Position)` - Uses stored world_ member

**Pop::try_spawn(Position)**
- Uses `shared_from_this()` to pass actual instance to world
- Locks weak_ptr to safely access world
- Ensures the SAME Pop object is referenced everywhere
- No duplicate instances created

**IAgent::sense()**
- No world parameter needed - uses stored world_ member
- Locks weak_ptr when accessing world data

## Previously Identified Issues (NOW RESOLVED)

### ✅ Issue 1: Object Lifetime Bug
**Problem**: `try_spawn()` created new Pop with `std::make_shared<Pop>(*this)`  
**Solution**: Use `shared_from_this()` to pass the actual instance

### ✅ Issue 2: Circular Ownership
**Problem**: Both World and AgentsManager owned entities via shared_ptr  
**Solution**: AgentsManager owns (shared_ptr), World observes (weak_ptr)

### ✅ Issue 3: Interface Inconsistency
**Problem**: Unclear ownership semantics in interfaces  
**Solution**: Added documentation clarifying World doesn't take ownership

### ✅ Issue 4: Unsafe Raw Pointers
**Problem**: World used raw pointers which could dangle  
**Solution**: Use weak_ptr for safe non-owning references

## Benefits of Current Architecture

1. **Memory Safety**: No dangling pointers, automatic cleanup via weak_ptr
2. **Clear Ownership**: Single source of truth (AgentsManager)
3. **RAII Compliance**: Proper resource management through smart pointers
4. **Testability**: Clear interfaces with well-defined contracts
5. **Maintainability**: Easy to understand who owns what
6. **Extensibility**: Easy to add new entity types following the same pattern
