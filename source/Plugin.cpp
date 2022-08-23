/* Plugin.cpp
Copyright (c) 2022 by quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Plugin.h"

#include "DataFile.h"
#include "DataWriter.h"
#include "Editor.h"
#include "Files.h"
#include "TemplateEditor.h"

#include <cassert>
#include <string>
#include <type_traits>

using namespace std;


namespace {
}



void Plugin::Load(const Editor &editor, string_view path)
{
	data.clear();
	unknownNodes.clear();
	hasModifications = false;

	// We assume that path refers to a valid path to the root of the plugin.
	auto files = Files::RecursiveList(string(path));
	for(const auto &file : files)
	{
		const auto filename = file.substr(path.size());
		for(auto &node : DataFile(file))
		{
			const string &key = node.Token(0);
			if(node.Size() < 2)
				continue;
			const auto &value = node.Token(1);

			if(key == "effect")
				data[filename].emplace_back(*effects.insert(editor.Universe().effects.Get(value)).first);
			else if(key == "fleet")
				data[filename].emplace_back(*fleets.insert(editor.Universe().fleets.Get(value)).first);
			else if(key == "galaxy")
				data[filename].emplace_back(*galaxies.insert(editor.Universe().galaxies.Get(value)).first);
			else if(key == "hazard")
				data[filename].emplace_back(*hazards.insert(editor.Universe().hazards.Get(value)).first);
			else if(key == "government")
				data[filename].emplace_back(*governments.insert(editor.Universe().governments.Get(value)).first);
			else if(key == "outfit")
				data[filename].emplace_back(*outfits.insert(editor.Universe().outfits.Get(value)).first);
			else if(key == "outfitter")
				data[filename].emplace_back(*outfitters.insert(editor.Universe().outfitSales.Get(value)).first);
			else if(key == "planet")
				data[filename].emplace_back(*planets.insert(editor.Universe().planets.Get(value)).first);
			else if(key == "ship")
				data[filename].emplace_back(*ships.insert(editor.Universe().ships.Get(value)).first);
			else if(key == "shipyard")
				data[filename].emplace_back(*shipyards.insert(editor.Universe().shipSales.Get(value)).first);
			else if(key == "system")
				data[filename].emplace_back(*systems.insert(editor.Universe().systems.Get(value)).first);
			else
			{
				unknownNodes.emplace_back(std::move(node));
				data[filename].emplace_back(&unknownNodes.back());
			}
		}
	}
}



void Plugin::Save(const Editor &editor, string_view path)
{
	for(const auto &[file, objects] : data)
	{
		DataWriter writer(string(path) + file);
		for(const auto &object : objects)
		{
			if(auto *effect = get_if<const Effect *>(&object); effect && effects.count(*effect))
				editor.effectEditor.WriteToFile(writer, *effect);
			else if(auto *fleet = get_if<const Fleet *>(&object); fleet && fleets.count(*fleet))
				editor.fleetEditor.WriteToFile(writer, *fleet);
			else if(auto *galaxy = get_if<const Galaxy *>(&object); galaxy && galaxies.count(*galaxy))
				editor.galaxyEditor.WriteToFile(writer, *galaxy);
			else if(auto *hazard = get_if<const Hazard *>(&object); hazard && hazards.count(*hazard))
				editor.hazardEditor.WriteToFile(writer, *hazard);
			else if(auto *government = get_if<const Government *>(&object); government && governments.count(*government))
				editor.governmentEditor.WriteToFile(writer, *government);
			else if(auto *outfit = get_if<const Outfit *>(&object); outfit && outfits.count(*outfit))
				editor.outfitEditor.WriteToFile(writer, *outfit);
			else if(auto *outfitter = get_if<const Sale<Outfit> *>(&object); outfitter && outfitters.count(*outfitter))
				editor.outfitterEditor.WriteToFile(writer, *outfitter);
			else if(auto *planet = get_if<const Planet *>(&object); planet && planets.count(*planet))
				editor.planetEditor.WriteToFile(writer, *planet);
			else if(auto *ship = get_if<const Ship *>(&object); ship && ships.count(*ship))
				editor.shipEditor.WriteToFile(writer, *ship);
			else if(auto *shipyard = get_if<const Sale<Ship> *>(&object); shipyard && shipyards.count(*shipyard))
				editor.shipyardEditor.WriteToFile(writer, *shipyard);
			else if(auto *system = get_if<const System *>(&object); system && systems.count(*system))
				editor.systemEditor.WriteToFile(writer, *system);
			else if(auto *node = get_if<const DataNode *>(&object))
				writer.Write(**node);

			writer.Write();
		}
	}
	hasModifications = false;
}



bool Plugin::HasChanges() const
{
	return hasModifications;
}



void Plugin::Add(Node node)
{
	hasModifications = true;

	// If the modified node is already present, we don't need to add it.
	if(Has(node))
		return;

	string file;
	switch(node.index())
	{
	case 0: file = defaultFileFor<Effect>(); effects.insert(*get_if<const Effect *>(&node)); break;
	case 1: file = defaultFileFor<Fleet>(); fleets.insert(*get_if<const Fleet *>(&node)); break;
	case 2: file = defaultFileFor<Galaxy>(); galaxies.insert(*get_if<const Galaxy *>(&node)); break;
	case 3: file = defaultFileFor<Hazard>(); hazards.insert(*get_if<const Hazard *>(&node)); break;
	case 4: file = defaultFileFor<Government>(); governments.insert(*get_if<const Government *>(&node)); break;
	case 5: file = defaultFileFor<Outfit>(); outfits.insert(*get_if<const Outfit *>(&node)); break;
	case 6: file = defaultFileFor<Sale<Outfit>>(); outfitters.insert(*get_if<const Sale<Outfit> *>(&node)); break;
	case 7: file = defaultFileFor<Planet>(); planets.insert(*get_if<const Planet *>(&node)); break;
	case 8: file = defaultFileFor<Ship>(); ships.insert(*get_if<const Ship *>(&node)); break;
	case 9: file = defaultFileFor<Sale<Ship>>(); shipyards.insert(*get_if<const Sale<Ship> *>(&node)); break;
	case 10: file = defaultFileFor<System>(); systems.insert(*get_if<const System *>(&node)); break;
	default: assert(!"no default file for node!"); break;
	}

	data[file].emplace_back(std::move(node));
}



bool Plugin::Has(const Node &node) const
{
	if(auto *effect = get_if<const Effect *>(&node))
		return effects.count(*effect);
	else if(auto *fleet = get_if<const Fleet *>(&node))
		return fleets.count(*fleet);
	else if(auto *galaxy = get_if<const Galaxy *>(&node))
		return galaxies.count(*galaxy);
	else if(auto *hazard = get_if<const Hazard *>(&node))
		return hazards.count(*hazard);
	else if(auto *government = get_if<const Government *>(&node))
		return governments.count(*government);
	else if(auto *outfit = get_if<const Outfit *>(&node))
		return outfits.count(*outfit);
	else if(auto *outfitter = get_if<const Sale<Outfit> *>(&node))
		return outfitters.count(*outfitter);
	else if(auto *planet = get_if<const Planet *>(&node))
		return planets.count(*planet);
	else if(auto *ship = get_if<const Ship *>(&node))
		return ships.count(*ship);
	else if(auto *shipyard = get_if<const Sale<Ship> *>(&node))
		return shipyards.count(*shipyard);
	else if(auto *system = get_if<const System *>(&node))
		return systems.count(*system);
	assert(!"can't count this node!");
	return false;
}



void Plugin::Remove(const Node &node)
{
	hasModifications = true;
	if(auto *effect = get_if<const Effect *>(&node))
		effects.erase(*effect);
	else if(auto *fleet = get_if<const Fleet *>(&node))
		fleets.erase(*fleet);
	else if(auto *galaxy = get_if<const Galaxy *>(&node))
		galaxies.erase(*galaxy);
	else if(auto *hazard = get_if<const Hazard *>(&node))
		hazards.erase(*hazard);
	else if(auto *government = get_if<const Government *>(&node))
		governments.erase(*government);
	else if(auto *outfit = get_if<const Outfit *>(&node))
		outfits.erase(*outfit);
	else if(auto *outfitter = get_if<const Sale<Outfit> *>(&node))
		outfitters.erase(*outfitter);
	else if(auto *planet = get_if<const Planet *>(&node))
		planets.erase(*planet);
	else if(auto *ship = get_if<const Ship *>(&node))
		ships.erase(*ship);
	else if(auto *shipyard = get_if<const Sale<Ship> *>(&node))
		shipyards.erase(*shipyard);
	else if(auto *system = get_if<const System *>(&node))
		systems.erase(*system);
	else
		assert(!"can't remove this node!");
}
