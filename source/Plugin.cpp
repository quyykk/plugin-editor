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

using namespace std;

namespace {

template <typename T>
auto &GetEditorForNodeElement(const Editor &editor)
{
	if constexpr(std::is_same_v<T, const Effect *>)
		return editor.effectEditor;
	else if constexpr(std::is_same_v<T, const Fleet *>)
		return editor.fleetEditor;
	else if constexpr(std::is_same_v<T, const Galaxy *>)
		return editor.galaxyEditor;
	else if constexpr(std::is_same_v<T, const Hazard *>)
		return editor.hazardEditor;
	else if constexpr(std::is_same_v<T, const Government *>)
		return editor.governmentEditor;
	else if constexpr(std::is_same_v<T, const Outfit *>)
		return editor.outfitEditor;
	else if constexpr(std::is_same_v<T, const Sale<Outfit> *>)
		return editor.outfitterEditor;
	else if constexpr(std::is_same_v<T, const Planet *>)
		return editor.planetEditor;
	else if constexpr(std::is_same_v<T, const Ship *>)
		return editor.shipEditor;
	else if constexpr(std::is_same_v<T, const Sale<Ship> *>)
		return editor.shipyardEditor;
	else if constexpr(std::is_same_v<T, const System *>)
		return editor.systemEditor;
	else
		assert(!"no editor for T");
}

}


void Plugin::Load(const Editor &editor, string_view path)
{
	data.clear();
	unknownNodes.clear();
	filesChanged.clear();
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
				data[filename].emplace_back(effects.emplace(editor.Universe().effects.Get(value), filename).first->first);
			else if(key == "fleet")
				data[filename].emplace_back(fleets.emplace(editor.Universe().fleets.Get(value), filename).first->first);
			else if(key == "galaxy")
				data[filename].emplace_back(galaxies.emplace(editor.Universe().galaxies.Get(value), filename).first->first);
			else if(key == "hazard")
				data[filename].emplace_back(hazards.emplace(editor.Universe().hazards.Get(value), filename).first->first);
			else if(key == "government")
				data[filename].emplace_back(governments.emplace(editor.Universe().governments.Get(value), filename).first->first);
			else if(key == "outfit")
				data[filename].emplace_back(outfits.emplace(editor.Universe().outfits.Get(value), filename).first->first);
			else if(key == "outfitter")
				data[filename].emplace_back(outfitters.emplace(editor.Universe().outfitSales.Get(value), filename).first->first);
			else if(key == "planet")
				data[filename].emplace_back(planets.emplace(editor.Universe().planets.Get(value), filename).first->first);
			else if(key == "ship")
				data[filename].emplace_back(ships.emplace(editor.Universe().ships.Get(value), filename).first->first);
			else if(key == "shipyard")
				data[filename].emplace_back(shipyards.emplace(editor.Universe().shipSales.Get(value), filename).first->first);
			else if(key == "system")
				data[filename].emplace_back(systems.emplace(editor.Universe().systems.Get(value), filename).first->first);
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
		// Skip files that haven't been modified by this plugin editor.
		if(!filesChanged.count(file))
			continue;

		DataWriter writer(string(path) + file);
		for(const auto &object : objects)
		{
			std::visit([this, &writer, &editor](const auto *ptr)
				{
					if constexpr(std::is_same_v<decltype(ptr), const DataNode *>)
						writer.Write(*ptr);
					else
					{
						const auto &map = GetMapForNodeElement<decltype(ptr)>();
						if(map.count(ptr))
							GetEditorForNodeElement<decltype(ptr)>(editor).WriteToFile(writer, ptr);
					}
				}, object);

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
	if(std::visit([this](const auto *ptr)
			{
				if constexpr(!std::is_same_v<decltype(ptr), const DataNode *>)
				{
					const auto &map = GetMapForNodeElement<decltype(ptr)>();
					auto it = map.find(ptr);
					if(it != map.end())
					{
						// But we do need to save that we have touch a node in this file.
						filesChanged.insert(it->second);
						return true;
					}
				}
				else
					assert(!"can't add a DataNode");

				return false;
			}, node))
		return;

	string file;
	std::visit([this, &file](const auto *ptr)
		{
			if constexpr(!std::is_same_v<decltype(ptr), const DataNode *>)
			{
				file = defaultFileFor<std::decay_t<std::remove_pointer_t<decltype(ptr)>>>();
				GetMapForNodeElement<decltype(ptr)>().emplace(ptr, file);
			}
			else
				assert(!"can't add a DataNode");
		}, node);

	data[file].emplace_back(std::move(node));
	filesChanged.insert(file);
}



bool Plugin::Has(const Node &node) const
{
	return std::visit([this](const auto *ptr)
		{
			return GetMapForNodeElement<decltype(ptr)>().count(ptr);
		}, node);
}



void Plugin::Remove(const Node &node)
{
	hasModifications = true;
	std::visit([this](const auto *ptr)
		{
			GetMapForNodeElement<decltype(ptr)>().erase(ptr);
		}, node);
}
