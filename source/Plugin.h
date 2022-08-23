/* Plugin.h
Copyright (c) 2022 by quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef PLUGIN_H_
#define PLUGIN_H_

#include "Sale.h"

#include <list>
#include <set>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

class DataNode;
class Editor;
class Effect;
class Fleet;
class Galaxy;
class Hazard;
class Government;
class Outfit;
class Planet;
class Ship;
class System;


// This class stores a list of objects that the plugin contains.
class Plugin {
public:
	using Node = std::variant<const Effect *, const Fleet *, const Galaxy *, const Hazard *,
		  const Government *, const Outfit *, const Sale<Outfit> *, const Planet *, const Ship *,
		  const Sale<Ship> *, const System *, const DataNode *>;


public:
	// Loads the plugin at the specified path.
	void Load(const Editor &editor, std::string_view path);
	// Saves this plugin to the specified path.
	void Save(const Editor &editor, std::string_view path);

	// Whether any changes were made to this plugin since loading it.
	bool HasChanges() const;

	// Adds the specified object as part of this plugin.
	void Add(Node node);

	// Checks if the specified object is part of this plugin.
	bool Has(const Node &node) const;

	// Removes the specified object as part of this plugin.
	void Remove(const Node &node);

private:
	// Whether this plugin had modifications made to it since calling Load.
	// Saving the plugin clears this flag.
	bool hasModifications = false;

	std::unordered_map<std::string, std::vector<Node>> data;
	std::list<DataNode> unknownNodes;

	std::set<const Effect *> effects;
	std::set<const Fleet *> fleets;
	std::set<const Galaxy *> galaxies;
	std::set<const Hazard *> hazards;
	std::set<const Government *> governments;
	std::set<const Outfit *> outfits;
	std::set<const Sale<Outfit> *> outfitters;
	std::set<const Planet *> planets;
	std::set<const Ship *> ships;
	std::set<const Sale<Ship> *> shipyards;
	std::set<const System *> systems;
};



#endif
