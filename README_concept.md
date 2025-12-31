Strutttura
========
IEntity <-- IAgent  <--- Pop
Pop is an implementation of IAgent representing a single individual in the simulation.
IEntity and IAgents  requires a pointer to IWorld for some methods
IAgentsManager <-- AgentsManager 
AgentsManager is in charge of managing the population of IAgents, spawn new ones, remove dead ones, update them each tick, etc.
IWorld <-- GridWorld
IWorld the map in its topology and for each cell has a list of IEntity present
IWorld requires a pointer to a specific IEntity for some methods, like adding or removing an entity.
ISimulator <-- Simulator
IRenderer <-- Renderer
Simulator and renderer act together in a loop, moving on the simulation tick by tick, updating the world and rendering it.
Simulator Owns a pointer to IWorld and IAgentsManager, and for each loop calls the update methods of both.
Renderer owns a pointer to IWorld and is in charge of rendering it.



Error
=====
IWorld and IAgentsManager have circular dependencies, as IWorld needs to add and remove IEntity (IAgents) and IAgentsManager needs to access the world topology.
Solution can be to let manager owns the entities and the world only have weak pointers to them or just ID
