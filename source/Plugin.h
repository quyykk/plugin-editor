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

#include <cassert>
#include <list>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
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
	// Helper function that returns the map corresponding to the type of ptr.
	template <typename T>
	std::map<T, std::string> &GetMapForNodeElement();
	template <typename T>
	const std::map<T, std::string> &GetMapForNodeElement() const;


private:
	// Whether this plugin had modifications made to it since calling Load.
	// Saving the plugin clears this flag.
	bool hasModifications = false;

	// List of data files that have modifications. This is to avoid rewriting files
	// that haven't been touched.
	std::set<std::string> filesChanged;

	std::unordered_map<std::string, std::vector<Node>> data;
	std::list<DataNode> unknownNodes;

	std::map<const Effect *, std::string> effects;
	std::map<const Fleet *, std::string> fleets;
	std::map<const Galaxy *, std::string> galaxies;
	std::map<const Hazard *, std::string> hazards;
	std::map<const Government *, std::string> governments;
	std::map<const Outfit *, std::string> outfits;
	std::map<const Sale<Outfit> *, std::string> outfitters;
	std::map<const Planet *, std::string> planets;
	std::map<const Ship *, std::string> ships;
	std::map<const Sale<Ship> *, std::string> shipyards;
	std::map<const System *, std::string> systems;
};



template <typename T>
std::map<T, std::string> &Plugin::GetMapForNodeElement()
{
	const auto &This = *this;
	return const_cast<std::map<T, std::string> &>(This.GetMapForNodeElement<T>());
}



template <typename T>
const std::map<T, std::string> &Plugin::GetMapForNodeElement() const
{
	if constexpr(std::is_same_v<T, const Effect *>)
		return effects;
	else if constexpr(std::is_same_v<T, const Fleet *>)
		return fleets;
	else if constexpr(std::is_same_v<T, const Galaxy *>)
		return galaxies;
	else if constexpr(std::is_same_v<T, const Hazard *>)
		return hazards;
	else if constexpr(std::is_same_v<T, const Government *>)
		return governments;
	else if constexpr(std::is_same_v<T, const Outfit *>)
		return outfits;
	else if constexpr(std::is_same_v<T, const Sale<Outfit> *>)
		return outfitters;
	else if constexpr(std::is_same_v<T, const Planet *>)
		return planets;
	else if constexpr(std::is_same_v<T, const Ship *>)
		return ships;
	else if constexpr(std::is_same_v<T, const Sale<Ship> *>)
		return shipyards;
	else if constexpr(std::is_same_v<T, const System *>)
		return systems;
	else
	{
		assert(!"no map for T");
		return {};
	}
}


#endif
