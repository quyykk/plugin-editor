/* SystemEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "SystemEditor.h"

#include "DataFile.h"
#include "DataWriter.h"
#include "Editor.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "Government.h"
#include "Hazard.h"
#include "MainEditorPanel.h"
#include "MapEditorPanel.h"
#include "Minable.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"
#include "Visual.h"

#include <random>

using namespace std;


namespace {

constexpr double STELLAR_MASS_SCALE = 6.25;

double GetMass(const StellarObject &stellar)
{
	constexpr double STAR_MASS_SCALE = .25;
	const auto radius = stellar.Radius();
	return radius * radius * STAR_MASS_SCALE;
}

// Habitable ranges for a given star.
std::map<std::string_view, double> starHabitableValues = {
	{ "star/o-supergiant", 33450. },
	{ "star/o-giant", 22300. },
	{ "star/o0", 13720. },
	{ "star/o3", 11500. },
	{ "star/o5", 10000. },
	{ "star/o8", 8650. },
	{ "star/o-dwarf", 1325. },
	{ "star/b-supergiant", 17025. },
	{ "star/b-giant", 11350. },
	{ "star/b0", 7000. },
	{ "star/b3", 6300. },
	{ "star/b5", 5600. },
	{ "star/b8", 5000. },
	{ "star/b-dwarf", 1125. },
	{ "star/a-supergiant", 11850. },
	{ "star/a-giant", 7900. },
	{ "star/a0", 3650. },
	{ "star/a3", 3400. },
	{ "star/a5", 3200. },
	{ "star/a8", 3000. },
	{ "star/a-dwarf", 750. },
	{ "star/f-supergiant", 8400. },
	{ "star/f-giant", 5600. },
	{ "star/f0", 2560. },
	{ "star/f3", 2200. },
	{ "star/f5", 1715. },
	{ "star/f5-old", 3430. },
	{ "star/f8", 1310. },
	{ "star/f-dwarf", 355. },
	{ "star/g-supergiant", 6075. },
	{ "star/g-giant", 4050. },
	{ "star/g0", 1080. },
	{ "star/g0-old", 2160. },
	{ "star/g3", 700. },
	{ "star/g5", 625. },
	{ "star/g5-old", 1250. },
	{ "star/g8", 550. },
	{ "star/g-dwarf", 150. },
	{ "star/k-supergiant", 4500. },
	{ "star/k-giant", 3000. },
	{ "star/k0", 490. },
	{ "star/k0-old", 980. },
	{ "star/k3", 450. },
	{ "star/k5", 425. },
	{ "star/k5-old", 950. },
	{ "star/k8", 370. },
	{ "star/k-dwarf", 100. },
	{ "star/m-supergiant", 3450. },
	{ "star/m-giant", 2300. },
	{ "star/m0", 320. },
	{ "star/m3", 230. },
	{ "star/m5", 160. },
	{ "star/m8", 135. },
	{ "star/m-dwarf", 35. },
	{ "star/l-dwarf", 30. },
	{ "planet/browndwarf-l-rouge", 10. },
	{ "planet/browndwarf-t-rouge", 10. },
	{ "planet/browndwarf-y-rouge", 10. },
	{ "star/wr", 50000. },
	{ "star/carbon", 3100. },
	{ "star/nova", 100. },
	{ "star/nova-small", 100. },
	{ "star/nova-old", 100. },
	{ "star/neutron", 100. },
	{ "star/black-hole-core", 10000. },
};


}

SystemEditor::SystemEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<System>(editor, show), gen(rd())
{
}



void SystemEditor::UpdateSystemPosition(const System *system, Point dp)
{
	const_cast<System *>(system)->position += dp;
	SetDirty(system);
}



void SystemEditor::UpdateStellarPosition(const StellarObject &object, Point pos, const System *system)
{
	auto &obj = const_cast<StellarObject &>(object);
	obj.distance = pos.Length();
	SetDirty(this->object);
}



void SystemEditor::ToggleLink(const System *system)
{
	if(system == object)
		return;

	auto it = object->links.find(system);
	if(it != object->links.end())
		object->Unlink(const_cast<System *>(system));
	else
		object->Link(const_cast<System *>(system));
	UpdateMap();
	SetDirty();
	SetDirty(system);
}



void SystemEditor::CreateNewSystem(Point position)
{
	this->position = position;
	createNewSystem = true;
}



void SystemEditor::CloneSystem(Point position)
{
	this->position = position;
	cloneSystem = true;
}



void SystemEditor::Delete(const StellarObject &stellar, bool selectedObject)
{
	auto it = find(object->objects.begin(), object->objects.end(), stellar);
	assert(it != object->objects.end() && "stellar doesn't belong to the current system");

	int selectedIndex = -1;
	if(this->selectedObject)
	{
		auto it = find(object->objects.begin(), object->objects.end(), *this->selectedObject);
		if(it != object->objects.end())
			selectedIndex = it - object->objects.begin();
	}

	if(auto *planet = stellar.planet)
		const_cast<Planet *>(planet)->RemoveSystem(object);
	SetDirty();
	auto index = it - object->objects.begin();
	auto next = object->objects.erase(it);
	size_t removed = 1;
	// Remove any child objects too.
	while(next != object->objects.end() && next->Parent() == index)
	{
		if(next->planet)
			const_cast<Planet *>(next->planet)->RemoveSystem(object);
		next = object->objects.erase(next);
		++removed;
	}

	// Recalculate every parent index.
	for(auto it = next; it != object->objects.end(); ++it)
		if(it->parent >= index)
			it->parent -= removed;

	if(selectedObject)
	{
		editor.SystemViewPanel()->DeselectObject();
		this->selectedObject = nullptr;
	}
	else if(selectedIndex > -1)
	{
		if(selectedIndex > next - object->objects.begin())
			selectedIndex -= removed;
		const auto &obj = *(object->objects.begin() + selectedIndex);
		editor.SystemViewPanel()->SelectObject(obj);
		this->selectedObject = &obj;
	}
}



void SystemEditor::Render()
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("System Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	if(ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
		RandomizeAll();
	if(ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
		GenerateTrades();

	bool showNewSystem = false;
	bool showRenameSystem = false;
	bool showCloneSystem = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("System"))
		{
			const bool alreadyDefined = object && !editor.BaseUniverse().systems.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewSystem);
			ImGui::MenuItem("Rename", nullptr, &showRenameSystem, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneSystem, object);
			if(ImGui::MenuItem("Reset", nullptr, false, alreadyDefined && editor.GetPlugin().Has(object)))
			{
				auto oldLinks = object->links;
				for(auto &&link : oldLinks)
				{
					const_cast<System *>(link)->Unlink(object);
					SetDirty(link);
				}
				for(auto &&stellar : object->Objects())
					if(stellar.planet)
						const_cast<Planet *>(stellar.planet)->RemoveSystem(object);

				auto oldNeighbors = object->VisibleNeighbors();

				editor.GetPlugin().Remove(object);
				*object = *editor.BaseUniverse().systems.Get(object->name);

				for(auto &&link : object->links)
					const_cast<System *>(link)->Link(object);
				for(auto &&stellar : object->Objects())
					if(stellar.planet)
						const_cast<Planet *>(stellar.planet)->SetSystem(object);
				UpdateMap();
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				Delete(object, true);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Randomize"))
		{
			if(ImGui::MenuItem("Randomize Stellars", nullptr, false, object))
				Randomize();
			if(ImGui::MenuItem("Randomize Asteroids", nullptr, false, object))
				RandomizeAsteroids();
			if(ImGui::MenuItem("Randomize Minables", nullptr, false, object))
				RandomizeMinables();
			ImGui::Separator();
			if(ImGui::MenuItem("Randomize All", "Ctrl+R", false, object))
				RandomizeAll();
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Tools"))
		{
			if(ImGui::MenuItem("Generate Trades", "Ctrl+T", false, object))
				GenerateTrades();
			if(ImGui::MenuItem("Standardize System", nullptr, false, object))
				StandardizeSystem();
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showRenameSystem)
		ImGui::OpenPopup("Rename System");
	AlwaysRender(showNewSystem, showCloneSystem);
	ImGui::BeginSimpleRenameModal("Rename System", [this](const string &name)
			{
				if(editor.Universe().systems.Find(name))
					return;

				editor.Universe().systems.Rename(object->name, name);
				object->name = name;
				UpdateMap();
				SetDirty();
			});

	if(editor.GetUI().Top() == editor.MapPanel())
		object = const_cast<System *>(editor.MapPanel()->Selected());
	if(editor.GetUI().Top() == editor.SystemViewPanel())
		object = const_cast<System *>(editor.SystemViewPanel()->Selected());

	if(ImGui::InputCombo("system", &searchBox, &object, editor.Universe().systems))
	{
		searchBox.clear();
		editor.MapPanel()->Select(object);
		editor.SystemViewPanel()->Select(object);
	}

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::PushID(object);
	if(object)
		RenderSystem();
	ImGui::PopID();
	ImGui::End();
}



void SystemEditor::AlwaysRender(bool showNewSystem, bool showCloneSystem)
{
	if(showNewSystem || createNewSystem)
		ImGui::OpenPopup("New System");
	ImGui::BeginSimpleNewModal("New System", [this](const string &name)
			{
				if(editor.Universe().systems.Find(name))
					return;

				auto *newSystem = editor.Universe().systems.Get(name);

				newSystem->name = name;
				newSystem->position = createNewSystem ? position : object->position + Point(25., 25.);
				newSystem->attributes.insert("uninhabited");
				newSystem->isDefined = true;
				newSystem->hasPosition = true;
				object = newSystem;
				UpdateMap();
				SetDirty();
				editor.MapPanel()->Select(object);
				editor.SystemViewPanel()->Select(object);
			});
	createNewSystem &= ImGui::IsPopupOpen("New System");

	if(showCloneSystem || cloneSystem)
		ImGui::OpenPopup("Clone System");
	ImGui::BeginSimpleCloneModal("Clone System", [this](const string &name)
			{
				if(editor.Universe().systems.Find(name))
					return;

				auto *clone = editor.Universe().systems.Get(name);
				*clone = *object;
				object = clone;

				object->name = name;
				object->position = cloneSystem ? position : object->position + Point(25., 25.);
				object->objects.clear();
				object->links.clear();
				object->attributes.insert("uninhabited");
				UpdateMap();
				SetDirty();
				editor.MapPanel()->Select(object);
				editor.SystemViewPanel()->Select(object);
			});
	cloneSystem &= ImGui::IsPopupOpen("Clone System");
}



void SystemEditor::RenderSystem()
{
	int index = 0;

	ImGui::Text("name: %s", object->name.c_str());
	if(ImGui::Checkbox("hidden", &object->hidden))
		SetDirty();

	double pos[2] = {object->position.X(), object->Position().Y()};
	if(ImGui::InputDouble2Ex("pos", pos))
	{
		object->position.Set(pos[0], pos[1]);
		UpdateMap();
		SetDirty();
	}

	{
		static Government *selected;
		string govName = object->government ? object->government->TrueName() : "";
		if(ImGui::InputCombo("government", &govName, &selected, editor.Universe().governments))
		{
			object->government = selected;
			UpdateMap();
			SetDirty();
		}
	}

	if(ImGui::InputText("music", &object->music))
		SetDirty();

	if(ImGui::InputDoubleEx("habitable", &object->habitable))
		SetDirty();
	if(ImGui::InputDoubleEx("jump range", &object->jumpRange))
	{
		editor.MapPanel()->UpdateJumpDistance();
		SetDirty();
	}
	if(object->jumpRange < 0.)
		object->jumpRange = 0.;
	string enterHaze = object->haze ? object->haze->Name() : "";
	if(ImGui::InputCombo("haze", &enterHaze, &object->haze, editor.Sprites()))
	{
		UpdateMain();
		SetDirty();
	}

	double arrival[2] = {object->extraHyperArrivalDistance, object->extraJumpArrivalDistance};
	if(ImGui::InputDouble2Ex("arrival", arrival))
		SetDirty();
	object->extraHyperArrivalDistance = arrival[0];
	object->extraJumpArrivalDistance = fabs(arrival[1]);

	if(ImGui::TreeNode("attributes"))
	{
		set<string> toAdd;
		set<string> toRemove;
		for(auto &attribute : object->attributes)
		{
			if(attribute == "uninhabited")
				continue;

			ImGui::PushID(index++);
			string str = attribute;
			if(ImGui::InputText("##attribute", &str, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if(!str.empty())
					toAdd.insert(move(str));
				toRemove.insert(attribute);
			}
			ImGui::PopID();
		}
		for(auto &&attribute : toAdd)
			object->attributes.insert(attribute);
		for(auto &&attribute : toRemove)
			object->attributes.erase(attribute);
		if(!toAdd.empty() || !toRemove.empty())
			SetDirty();

		ImGui::Spacing();

		static string addAttribute;
		if(ImGui::InputText("add attribute", &addAttribute, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->attributes.insert(move(addAttribute));
			SetDirty();
		}
		if(!addAttribute.empty() && !ImGui::IsInputFocused("add attribute"))
		{
			object->attributes.insert(move(addAttribute));
			SetDirty();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("links"))
	{
		set<System *> toAdd;
		set<System *> toRemove;
		index = 0;
		for(auto &link : object->links)
		{
			ImGui::PushID(index++);
			static System *newLink = nullptr;
			string name = link->Name();
			if(ImGui::InputCombo("##link", &name, &newLink, editor.Universe().systems))
			{
				if(newLink)
					toAdd.insert(newLink);
				newLink = nullptr;
				toRemove.insert(const_cast<System *>(link));
			}
			ImGui::PopID();
		}
		ImGui::Spacing();

		static System *newLink = nullptr;
		string addLink;
		if(ImGui::InputCombo("add link", &addLink, &newLink, editor.Universe().systems))
			if(newLink)
			{
				toAdd.insert(newLink);
				newLink = nullptr;
			}

		if(toAdd.count(object))
		{
			toAdd.erase(object);
			toRemove.erase(object);
		}
		for(auto &sys : toAdd)
		{
			object->Link(sys);
			SetDirty(sys);
		}
		for(auto &&sys : toRemove)
		{
			object->Unlink(sys);
			SetDirty(sys);
		}
		if(!toAdd.empty() || !toRemove.empty())
		{
			SetDirty();
			UpdateMap();
		}
		ImGui::TreePop();
	}

	bool asteroidsOpen = ImGui::TreeNode("asteroids");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Asteroid"))
		{
			object->asteroids.emplace_back("small rock", 1, 1.);
			UpdateMain();
			SetDirty();
		}
		if(ImGui::Selectable("Add Mineable"))
		{
			object->asteroids.emplace_back(&editor.Universe().minables.begin()->second, 1, 1.);
			UpdateMain();
			SetDirty();
		}
		ImGui::EndPopup();
	}

	if(asteroidsOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &asteroid : object->asteroids)
		{
			ImGui::PushID(index);
			if(asteroid.Type())
			{
				bool open = ImGui::TreeNode("minables", "mineable: %s %d %g", asteroid.Type()->Name().c_str(), asteroid.count, asteroid.energy);
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
						toRemove = index;
					ImGui::EndPopup();
				}

				if(open)
				{
					if(ImGui::BeginCombo("name", asteroid.Type()->Name().c_str()))
					{
						int index = 0;
						for(const auto &item : editor.Universe().minables)
						{
							const bool selected = &item.second == asteroid.Type();
							if(ImGui::Selectable(item.first.c_str(), selected))
							{
								asteroid.type = &item.second;
								UpdateMain();
								SetDirty();
							}
							++index;

							if(selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					if(ImGui::InputInt("count", &asteroid.count))
					{
						UpdateMain();
						SetDirty();
					}
					if(ImGui::InputDoubleEx("energy", &asteroid.energy))
					{
						UpdateMain();
						SetDirty();
					}
					ImGui::TreePop();
				}
			}
			else
			{
				bool open = ImGui::TreeNode("asteroids", "asteroid: %s %d %g", asteroid.name.c_str(), asteroid.count, asteroid.energy);
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
					{
						UpdateMain();
						toRemove = index;
					}
					ImGui::EndPopup();
				}

				if(open)
				{
					if(ImGui::InputText("name", &asteroid.name))
					{
						UpdateMain();
						SetDirty();
					}
					if(ImGui::InputInt("count", &asteroid.count))
					{
						UpdateMain();
						SetDirty();
					}
					if(ImGui::InputDoubleEx("energy", &asteroid.energy))
					{
						UpdateMain();
						SetDirty();
					}
					ImGui::TreePop();
				}
			}
			++index;
			ImGui::PopID();
		}

		if(toRemove != -1)
		{
			object->asteroids.erase(object->asteroids.begin() + toRemove);
			UpdateMain();
			SetDirty();
		}
		ImGui::TreePop();
	}

	bool beltOpen = ImGui::TreeNode("belts");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add belt"))
		{
			object->belts.emplace_back(1, 1500);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(beltOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &belt : object->belts)
		{
			ImGui::PushID(index);
			bool open = ImGui::TreeNode("belt", "%g %zu", belt.item, belt.weight);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
					toRemove = index;
				ImGui::EndPopup();
			}
			if(open)
			{
				if(ImGui::InputDoubleEx("radius", &belt.item))
					SetDirty();
				const int oldWeight = belt.weight;
				if(ImGui::InputSizeTEx("weight", &belt.weight))
				{
					if(belt.weight < 1)
						belt.weight = 1;
					object->belts.total -= belt.weight - oldWeight;
					SetDirty();
				}
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}
		if(toRemove != -1)
		{
			object->belts.eraseAt(object->belts.begin() + toRemove);
			SetDirty();
		}
		ImGui::TreePop();
	}
	// There must be a belt at all times.
	if(object->belts.empty())
		object->belts.emplace_back(1, 1500);

	bool fleetOpen = ImGui::TreeNode("fleets");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Fleet"))
		{
			object->fleets.emplace_back(&editor.Universe().fleets.begin()->second, 1);
			SetDirty();
		}
		ImGui::EndPopup();
	}

	if(fleetOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &fleet : object->fleets)
		{
			ImGui::PushID(index);
			bool open = ImGui::TreeNode("fleet", "%s %d", fleet.Get()->Name().c_str(), fleet.period);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
					toRemove = index;
				ImGui::EndPopup();
			}

			if(open)
			{
				static Fleet *selected;
				string fleetName = fleet.Get() ? fleet.Get()->Name() : "";
				if(ImGui::InputCombo("fleet", &fleetName, &selected, editor.Universe().fleets))
					if(selected)
					{
						fleet.event = selected;
						selected = nullptr;
						SetDirty();
					}
				if(ImGui::InputInt("period", &fleet.period, 100, 1000))
					SetDirty();
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}
		if(toRemove != -1)
		{
			object->fleets.erase(object->fleets.begin() + toRemove);
			SetDirty();
		}
		ImGui::TreePop();
	}

	RenderHazards(object->hazards);

	if(ImGui::TreeNode("trades"))
	{
		index = 0;
		for(auto &&commodity : editor.Universe().trade.Commodities())
		{
			ImGui::PushID(index++);
			int value = 0;
			auto it = object->trade.find(commodity.name);
			if(it != object->trade.end())
				value = it->second.base;
			if(ImGui::InputInt(commodity.name.c_str(), &value))
			{
				if(!value && it != object->trade.end())
					object->trade.erase(it);
				else if(value)
					object->trade[commodity.name].SetBase(value);
				editor.MapPanel()->UpdateCache();
				SetDirty();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	bool openObjects = ImGui::TreeNode("objects");
	bool openAddObject = false;
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Object"))
			openAddObject = true;
		ImGui::EndPopup();
	}
	if(openAddObject)
	{
		int selectedIndex = -1;
		if(this->selectedObject)
		{
			auto it = find(object->objects.begin(), object->objects.end(), *this->selectedObject);
			if(it != object->objects.end())
				selectedIndex = it - object->objects.begin();
		}
		object->objects.emplace_back();
		if(selectedIndex > -1)
		{
			const auto &obj = *(object->objects.begin() + selectedIndex);
			editor.SystemViewPanel()->SelectObject(obj);
			this->selectedObject = &obj;
		}
		SetDirty();
	}

	if(openObjects)
	{
		bool hovered = false;
		bool add = false;
		index = 0;
		int nested = 0;
		auto selected = object->objects.end();
		auto selectedToAdd = object->objects.end();
		for(auto it = object->objects.begin(); it != object->objects.end(); ++it)
		{
			ImGui::PushID(index);
			RenderObject(*it, index, nested, hovered, add);
			if(hovered)
			{
				selected = it;
				hovered = false;
			}
			if(add)
			{
				selectedToAdd = it;
				add = false;
			}
			++index;
			ImGui::PopID();
		}
		ImGui::TreePop();

		if(selected != object->objects.end())
			Delete(*selected, false);
		else if(selectedToAdd != object->objects.end())
		{
			int selectedIndex = -1;
			if(this->selectedObject)
			{
				auto it = find(object->objects.begin(), object->objects.end(), *this->selectedObject);
				if(it != object->objects.end())
					selectedIndex = it - object->objects.begin();
			}

			SetDirty();
			const int parent = selectedToAdd - object->objects.begin();
			auto it = object->objects.emplace(selectedToAdd + 1);
			it->parent = parent;

			auto nextObj = it;
			for(++it; it != object->objects.end(); ++it)
				if(it->parent > parent)
					++it->parent;

			// Recalculate selected object if necessary.
			if(selectedIndex >= nextObj - object->objects.begin())
				++selectedIndex;
			const auto &obj = *(object->objects.begin() + selectedIndex);
			editor.SystemViewPanel()->SelectObject(obj);
			this->selectedObject = &obj;
		}
	}
}



void SystemEditor::RenderObject(StellarObject &object, int index, int &nested, bool &hovered, bool &add)
{
	if(object.parent != -1 && !nested)
		return;

	string selectedString = &object == selectedObject ? "(selected)"
		: selectedObject && selectedObject->parent != -1
			&& &object == &this->object->objects[selectedObject->parent] ? "(child selected)"
		: "";
	string planetString =
		object.GetPlanet() ? object.GetPlanet()->TrueName() :
		object.GetSprite() ? object.GetSprite()->Name() : "";
	if(!planetString.empty() && !selectedString.empty())
		planetString += ' ';
	bool isOpen = ImGui::TreeNode("object", "object %s%s", planetString.c_str(), selectedString.c_str());

	ImGui::PushID(index);
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::MenuItem("Add Child", nullptr, false, object.parent == -1))
			add = true;
		if(ImGui::MenuItem("Remove"))
			hovered = true;
		ImGui::EndPopup();
	}
	ImGui::PopID();

	if(isOpen)
	{
		static Planet *planet = nullptr;
		static string planetName;
		planetName.clear();
		if(object.planet)
			planetName = object.planet->TrueName();
		if(ImGui::InputCombo("planet", &planetName, &planet, editor.Universe().planets))
		{
			if(object.planet)
				const_cast<Planet *>(object.planet)->RemoveSystem(this->object);
			object.planet = planet;
			if(planet)
			{
				planet->SetSystem(this->object);
				planet = nullptr;
			}
			SetDirty();
		}
		Sprite *sprite = nullptr;
		string spriteName;
		if(object.sprite)
			spriteName = object.sprite->Name();
		if(ImGui::InputCombo("sprite", &spriteName, &sprite, editor.Sprites(),
					[](const string &name) { return !name.compare(0, 7, "planet/")
					    || !name.compare(0, 5, "star/"); }))
		{
			object.sprite = sprite;
			object.isStation = (spriteName.find("station") != string::npos);
			SetDirty();
		}

		if(ImGui::InputDoubleEx("distance", &object.distance))
			SetDirty();
		double period = 0.;
		if(object.Speed())
			period = 360. / object.Speed();
		if(ImGui::InputDoubleEx("period", &period))
		{
			object.speed = period ? 360. / period : 0.;
			SetDirty();
		}
		if(ImGui::InputDoubleEx("offset", &object.offset))
			SetDirty();

		RenderHazards(object.hazards);

		if(index + 1 < static_cast<int>(this->object->objects.size()) && this->object->objects[index + 1].Parent() == index)
		{
			++nested; // If the next object is a child, don't close this tree node just yet.
			return;
		}
		else
			ImGui::TreePop();
	}

	// If are nested, then we need to remove this nesting until we are at the next desired level.
	if(nested && index + 1 >= static_cast<int>(this->object->objects.size()))
		while(nested--)
			ImGui::TreePop();
	else if(nested)
	{
		int nextParent = this->object->objects[index + 1].Parent();
		if(nextParent == object.Parent())
			return;
		while(nextParent != index)
		{
			nextParent = nextParent == -1 ? index : this->object->objects[nextParent].Parent();
			--nested;
			ImGui::TreePop();
		}
	}
}



void SystemEditor::RenderHazards(std::vector<RandomEvent<Hazard>> &hazards)
{
	bool hazardOpen = ImGui::TreeNode("hazards");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Hazard"))
			hazards.emplace_back(&editor.Universe().hazards.begin()->second, 1);
		ImGui::EndPopup();
	}

	if(hazardOpen)
	{
		int index = 0;
		int toRemove = -1;
		for(auto &hazard : hazards)
		{
			ImGui::PushID(index);
			bool open = ImGui::TreeNode("hazard", "hazard: %s %d", hazard.Get()->Name().c_str(), hazard.period);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
					toRemove = index;
				ImGui::EndPopup();
			}

			if(open)
			{
				static Hazard *selected;
				string hazardName = hazard.Get() ? hazard.Get()->Name() : "";
				if(ImGui::InputCombo("hazard", &hazardName, &selected, editor.Universe().hazards))
					if(selected)
					{
						hazard.event = selected;
						SetDirty();
					}
				if(ImGui::InputInt("period", &hazard.period))
					SetDirty();
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}

		if(toRemove != -1)
		{
			hazards.erase(hazards.begin() + toRemove);
			SetDirty();
		}
		ImGui::TreePop();
	}
}



void SystemEditor::WriteObject(DataWriter &writer, const System *system, const StellarObject *object, bool add) const
{
	// Calculate the nesting of this object. We follow parent indices until we reach
	// the root node.
	int i = object->Parent();
	int nested = 0;
	while(i != -1)
	{
		i = system->objects[i].Parent();
		++nested;
	}

	for(i = 0; i < nested; ++i)
		writer.BeginChild();

	if(add && !nested)
		writer.WriteToken("add");
	writer.WriteToken("object");

	if(object->GetPlanet())
		writer.WriteToken(object->GetPlanet()->TrueName());
	writer.Write();

	writer.BeginChild();
	if(object->GetSprite())
		writer.Write("sprite", object->GetSprite()->Name());
	if(object->distance)
		writer.Write("distance", object->Distance());
	if(object->speed)
		writer.Write("period", 360. / object->Speed());
	if(object->Offset())
		writer.Write("offset", object->Offset());
	for(const auto &hazard : object->hazards)
		writer.Write("hazard", hazard.Name(), hazard.Period());
	writer.EndChild();

	for(i = 0; i < nested; ++i)
		writer.EndChild();
}



void SystemEditor::WriteToFile(DataWriter &writer, const System *system) const
{
	const auto *diff = editor.BaseUniverse().systems.Has(system->name)
		? editor.BaseUniverse().systems.Get(system->name)
		: nullptr;

	writer.Write("system", system->name);
	writer.BeginChild();

	if(!diff || system->hidden != diff->hidden)
	{
		if(system->hidden)
			writer.Write("hidden");
		else if(diff)
			writer.Write("remove", "hidden");
	}
	if((!diff && system->hasPosition) || (diff && (system->hasPosition != diff->hasPosition || system->position != diff->position)))
		writer.Write("pos", system->position.X(), system->position.Y());
	if(!diff || system->government != diff->government)
	{
		if(system->government)
			writer.Write("government", system->government->TrueName());
		else if(diff)
			writer.Write("remove", "government");
	}

	auto systemAttributes = system->attributes;
	auto diffAttributes = diff ? diff->attributes : system->attributes;
	systemAttributes.erase("uninhabited");
	diffAttributes.erase("uninhabited");
	WriteDiff(writer, "attributes", systemAttributes, diff ? &diffAttributes : nullptr, true);

	if(!diff || system->music != diff->music)
	{
		if(!system->music.empty())
			writer.Write("music", system->music);
		else if(diff)
			writer.Write("remove", "music");
	}
	if(!diff || system->extraHyperArrivalDistance != diff->extraHyperArrivalDistance
			|| system->extraJumpArrivalDistance != diff->extraJumpArrivalDistance)
	{
		if(system->extraHyperArrivalDistance == system->extraJumpArrivalDistance
				&& (diff || system->extraHyperArrivalDistance))
			writer.Write("arrival", system->extraHyperArrivalDistance);
		else if(system->extraHyperArrivalDistance != system->extraJumpArrivalDistance)
		{
			writer.Write("arrival");
			writer.BeginChild();
			if((!diff && system->extraHyperArrivalDistance) || system->extraHyperArrivalDistance != diff->extraHyperArrivalDistance)
				writer.Write("link", system->extraHyperArrivalDistance);
			if((!diff && system->extraJumpArrivalDistance) || system->extraJumpArrivalDistance != diff->extraJumpArrivalDistance)
				writer.Write("jump", system->extraJumpArrivalDistance);
			writer.EndChild();
		}
	}
	if((!diff && system->habitable != 1000.) || (diff && system->habitable != diff->habitable))
		writer.Write("habitable", system->habitable);

	auto belts = system->belts;
	if(belts.size() == 1 && belts.back() == 1500)
		belts.eraseAt(belts.begin());
	auto diffBelts = diff ? diff->belts : system->belts;
	if(diffBelts.size() == 1 && diffBelts.back() == 1500)
		diffBelts.eraseAt(diffBelts.begin());
	WriteDiff(writer, "belt", belts, diff ? &diffBelts : nullptr);

	if((!diff && system->jumpRange) || (diff && system->jumpRange != diff->jumpRange))
		writer.Write("jump range", system->jumpRange);

	if(!diff || system->haze != diff->haze)
	{
		if(system->haze)
			writer.Write("haze", system->haze->Name());
		else if(diff)
			writer.Write("remove", "haze");
	}

	WriteDiff(writer, "link", system->links, diff ? &diff->links : nullptr, false, true, false, true);

	auto asteroids = system->asteroids;
	asteroids.erase(std::remove_if(asteroids.begin(), asteroids.end(), [](const System::Asteroid &a) { return a.Type(); }), asteroids.end());
	auto minables = system->asteroids;
	minables.erase(std::remove_if(minables.begin(), minables.end(), [](const System::Asteroid &a) { return !a.Type(); }), minables.end());
	auto diffAsteroids = diff ? diff->asteroids : system->asteroids;
	diffAsteroids.erase(std::remove_if(diffAsteroids.begin(), diffAsteroids.end(), [](const System::Asteroid &a) { return a.Type(); }), diffAsteroids.end());
	auto diffMinables = diff ? diff->asteroids : system->asteroids;
	diffMinables.erase(std::remove_if(diffMinables.begin(), diffMinables.end(), [](const System::Asteroid &a) { return !a.Type(); }), diffMinables.end());
	WriteDiff(writer, "asteroids", asteroids, diff ? &diffAsteroids : nullptr);
	WriteDiff(writer, "minables", minables, diff ? &diffMinables : nullptr);

	if(!diff || system->trade != diff->trade)
	{
		auto trades = system->trade;
		if(diff)
		{
			bool hasRemoved = false;
			for(auto it = diff->trade.begin(); it != diff->trade.end(); ++it)
				if(system->trade.find(it->first) == system->trade.end())
				{
					writer.Write("remove", "trade");
					hasRemoved = true;
					break;
				}
			if(!hasRemoved)
				for(auto it = trades.begin(); it != trades.end();)
				{
					auto jt = diff->trade.find(it->first);
					if(jt != diff->trade.end() && jt->second == it->second)
						it = trades.erase(it);
					else
						++it;
				}
		}

		if(!trades.empty())
			for(auto &&trade : trades)
				writer.Write("trade", trade.first, trade.second.base);
	}
	WriteDiff(writer, "fleet", system->fleets, diff ? &diff->fleets : nullptr);
	WriteDiff(writer, "hazard", system->hazards, diff ? &diff->hazards : nullptr);

	if(!diff || system->objects != diff->objects)
	{
		std::vector<const StellarObject *> toAdd;
		if(diff && system->objects.size() > diff->objects.size())
		{
			std::transform(system->objects.begin() + diff->objects.size(), system->objects.end(),
					back_inserter(toAdd), [](const StellarObject &obj) { return &obj; });

			for(int i = 0; i < static_cast<int>(diff->objects.size()); ++i)
				if(system->objects[i] != diff->objects[i])
				{
					toAdd.clear();
					break;
				}
		}

		if(!toAdd.empty())
			for(auto &&object : toAdd)
				WriteObject(writer, system, object, true);
		else if(!system->objects.empty())
			for(auto &&object : system->objects)
				WriteObject(writer, system, &object);
		else if(diff)
			writer.Write("remove object");
	}

	writer.EndChild();
}



void SystemEditor::Delete(const std::vector<const System *> &systems)
{
	for(const auto &system : systems)
		Delete(system, false);
}


void SystemEditor::RandomizeAll() {
	RandomizeAsteroids();
	RandomizeMinables();
	Randomize();
}



void SystemEditor::UpdateMap() const
{
	editor.MapPanel()->UpdateCache();
}



void SystemEditor::UpdateMain() const
{
	editor.SystemViewPanel()->UpdateCache();
}



void SystemEditor::Randomize()
{
	// Randomizes a star system.
	// Code adapted from the official ES map editor under GPL 3.0.
	// https://github.com/endless-sky/endless-sky-editor
	constexpr int STAR_DISTANCE = 40;
	static uniform_int_distribution<> randStarNum(0, 2);
	static uniform_int_distribution<> randStarDist(0, STAR_DISTANCE);

	auto getRadius = [](const Sprite *stellar)
	{
		return stellar->Width() / 2. - 4.;
	};

	editor.SystemViewPanel()->DeselectObject();
	selectedObject = nullptr;

	for(auto &stellar : object->objects)
		if(stellar.planet)
			const_cast<Planet *>(stellar.planet)->RemoveSystem(object);
	object->objects.clear();

	// First we generate the (or 2) star(s).
	const int numStars = 1 + !randStarNum(gen);
	if(numStars == 1)
	{
		StellarObject stellar;
		stellar.sprite = RandomStarSprite();
		stellar.speed = 36.;

		object->objects.push_back(stellar);
		object->habitable = starHabitableValues[stellar.sprite->Name()];
	}
	else
	{
		StellarObject stellar1;
		StellarObject stellar2;

		stellar1.isStar = true;
		stellar2.isStar = true;

		stellar1.sprite = RandomStarSprite();
		do stellar2.sprite = RandomStarSprite();
		while (stellar2.sprite == stellar1.sprite);

		stellar2.offset = 180.;

		double radius1 = getRadius(stellar1.sprite);
		double radius2 = getRadius(stellar2.sprite);

		// Make sure the 2 stars have similar radius.
		// If we fail to find a candidate after a lot of tries, it's fine.
		int attempts = 25;
		while(fabs(radius1 - radius2) > 100. && attempts)
		{
			const Sprite *sprite;
			do sprite = RandomStarSprite();
			while (sprite == stellar2.sprite);

			auto radius = getRadius(sprite);
			if(fabs(radius - radius2) < fabs(radius1 - radius2))
			{
				radius1 = getRadius(stellar1.sprite);
				stellar1.sprite = sprite;
			}
			--attempts;
		}

		double mass1 = GetMass(stellar1);
		double mass2 = GetMass(stellar2);
		double mass = mass1 + mass2;

		double distance = radius1 + radius2 + randStarDist(gen) + STAR_DISTANCE;
		stellar1.distance = (mass2 * distance) / mass;
		stellar2.distance = (mass1 * distance) / mass;

		double period = sqrt(distance * distance * distance / mass);
		stellar1.speed = 360. / period;
		stellar2.speed = 360. / period;

		if(radius1 > radius2)
			swap(stellar1, stellar2);
		object->objects.push_back(stellar1);
		object->objects.push_back(stellar2);

		auto habitable1 = starHabitableValues[stellar1.sprite->Name()];
		auto habitable2 = starHabitableValues[stellar2.sprite->Name()];
		object->habitable = sqrt(habitable1 * habitable1 + habitable2 * habitable2);
	}

	const double stellarMass = object->habitable * STELLAR_MASS_SCALE;

	// Now we generate lots of planets with moons.
	uniform_int_distribution<> randPlanetCount(4, 6);
	int planetCount = randPlanetCount(gen);
	for(int i = 0; i < planetCount; ++i)
	{
		constexpr int RANDOM_SPACE = 100;
		int space = RANDOM_SPACE;
		for(const auto &stellar : object->objects)
			if(!stellar.isStar && stellar.parent == -1)
				space += space / 2;

		int distance = object->objects.back().distance;
		if(object->objects.back().sprite)
			distance += getRadius(object->objects.back().sprite);
		if(object->objects.back().parent != -1)
			distance += object->objects[object->objects.back().parent].distance;

		uniform_int_distribution<> randSpace(RANDOM_SPACE, space);
		const int addSpace = randSpace(gen);
		distance += (addSpace * addSpace) * .01 + 50.;

		set<const Sprite *> used;
		for(const auto &stellar : object->objects)
			used.insert(stellar.sprite);

		uniform_int_distribution<> rand10(0, 9);
		uniform_int_distribution<> rand2000(0, 1999);
		const bool isHabitable = distance > object->habitable * .5 && distance < object->habitable * 2.;
		const bool isSmall = !rand10(gen);
		const bool isTerrestrial = !isSmall && rand2000(gen) > distance;

		const int rootIndex = static_cast<int>(object->objects.size());

		const Sprite *planetSprite;
		do {
			if(isSmall)
				planetSprite = RandomMoonSprite();
			else if(isTerrestrial)
				planetSprite = isHabitable ? RandomPlanetSprite() : RandomPlanetSprite();
			else
				planetSprite = RandomGiantSprite();
		} while(used.count(planetSprite));

		object->objects.emplace_back();
		object->objects.back().sprite = planetSprite;
		object->objects.back().isStation = false;
		used.insert(planetSprite);

		uniform_int_distribution<> oneOrTwo(1, 2);
		uniform_int_distribution<> twoOrFour(2, 4);

		const int randMoon = isTerrestrial ? oneOrTwo(gen) : twoOrFour(gen);
		int moonCount = uniform_int_distribution<>(0, randMoon - 1)(gen);
		if(getRadius(object->objects.back().sprite) < 70.)
			moonCount = 0;

		auto calcPeriod = [&stellarMass, &getRadius](StellarObject &stellar, bool isMoon)
		{
			if(isMoon)
			{
				const auto radius = getRadius(stellar.sprite);
				constexpr double PLANET_MASS_SCALE = .015;
				const auto mass = radius * radius * radius * PLANET_MASS_SCALE;
				stellar.speed = 360. / sqrt(stellar.distance * stellar.distance * stellar.distance / mass);
			}
			else
				stellar.speed = 360. / sqrt(stellar.distance * stellar.distance * stellar.distance / stellarMass);
		};

		double moonDistance = getRadius(object->objects.back().sprite);
		int randomMoonSpace = 75.;
		for(int i = 0; i < moonCount; ++i)
		{
			uniform_int_distribution<> randMoonDist(30., randomMoonSpace);
			moonDistance += randMoonDist(gen);
			randomMoonSpace += 20.;

			const Sprite *moonSprite;
			do moonSprite = RandomMoonSprite();
			while(used.count(moonSprite));
			used.insert(moonSprite);

			object->objects.emplace_back();
			object->objects.back().isMoon = true;
			object->objects.back().isStation = false;
			object->objects.back().sprite = moonSprite;
			object->objects.back().parent = rootIndex;
			object->objects.back().distance = moonDistance + getRadius(object->objects.back().sprite);
			calcPeriod(object->objects.back(), true);
			moonDistance += 2. * getRadius(object->objects.back().sprite);
		}

		object->objects[rootIndex].distance = distance + moonDistance;
		calcPeriod(object->objects[rootIndex], false);
	}

	SetDirty();

	// Update the new objects' positions here to avoid a flicker.
	editor.SystemViewPanel()->Step();
}



void SystemEditor::StandardizeSystem()
{
	if(object->objects.empty())
		return;

	// Calculate the habitable value.
	auto *star1 = object->objects.front().sprite;

	if(object->objects.size() == 1 || !object->objects[1].isStar)
		object->habitable = starHabitableValues[star1->Name()];
	else
	{
		auto habitable1 = starHabitableValues[star1->Name()];
		auto habitable2 = starHabitableValues[object->objects[1].sprite->Name()];
		object->habitable = sqrt(habitable1 * habitable1 + habitable2 * habitable2);
	}

	const auto stellarMass = object->habitable * STELLAR_MASS_SCALE;

	// Now calculate the periods of every star.
	for(size_t i = 2; i < object->objects.size(); ++i)
	{
		auto &stellar = object->objects[i];
		if(stellar.parent == -1)
			stellar.speed = 360. / sqrt(stellar.distance * stellar.distance * stellar.distance / stellarMass);
	}

	SetDirty();
}



void SystemEditor::RandomizeAsteroids()
{
	// Randomizes the asteroids in this system.
	// Code adapted from the official ES map editor under GPL 3.0.
	// https://github.com/endless-sky/endless-sky-editor
	object->asteroids.erase(remove_if(object->asteroids.begin(), object->asteroids.end(),
				[](const System::Asteroid &asteroid) { return !asteroid.Type(); }),
			object->asteroids.end());

	uniform_int_distribution<> rand(0, 21);
	const int total = rand(gen) * rand(gen) + 1;
	const double energy = (rand(gen) + 10) * (rand(gen) + 10) * .01;
	const string prefix[] = { "small", "medium", "large" };
	const char *suffix[] = { " rock", " metal" };

	uniform_int_distribution<> randCount(0, total - 1);
	int amount[] = { randCount(gen), 0 };
	amount[1] = total - amount[0];

	for(int i = 0; i < 2; ++i)
	{
		if(!amount[i])
			continue;

		uniform_int_distribution<> randCount(0, amount[i] - 1);
		int count[] = { 0, randCount(gen), 0 };
		int remaining = amount[i] - count[1];
		if(remaining)
		{
			uniform_int_distribution<> randRemaining(0, remaining - 1);
			count[0] = randRemaining(gen);
			count[2] = remaining - count[0];
		}

		for(int j = 0; j < 3; ++j)
			if(count[j])
			{
				uniform_int_distribution<> randEnergy(50, 100);
				object->asteroids.emplace_back(prefix[j] + suffix[i], count[j], energy * randEnergy(gen) * .01);
			}
	}

	UpdateMain();
	SetDirty();
}



void SystemEditor::RandomizeMinables()
{
	// Randomizes the minables in this system.
	// Code adapted from the official ES map editor under GPL 3.0.
	// https://github.com/endless-sky/endless-sky-editor
	object->asteroids.erase(remove_if(object->asteroids.begin(), object->asteroids.end(),
				[](const System::Asteroid &asteroid) { return asteroid.Type(); }),
			object->asteroids.end());

	// Generate a random number of asteroid belts. Systems generally have only one asteroid belt,
	// but rarely more.
	int beltCount = uniform_int_distribution<>(0, 10)(gen);
	if(beltCount <= 5)
		beltCount = 1;
	else if(beltCount <= 9)
		beltCount = 2;
	else
		beltCount = 3;

	object->belts.clear();

	int prev = 0;
	for(int i = 0; i < beltCount; ++i)
	{
		uniform_int_distribution<> randBelt(max(1000 + 500 * i, prev), 2000 + 500 * i);
		uniform_int_distribution<> randWeight(1, 10);
		object->belts.emplace_back(randWeight(gen), randBelt(gen));

		prev =object->belts.back();
	}

	int totalCount = 0;
	double totalEnergy = 0.;
	for(const auto &asteroid : object->asteroids)
	{
		totalCount += asteroid.count;
		totalEnergy += asteroid.energy * asteroid.count;
	}

	if(!totalCount)
	{
		// This system has no other asteroids, so we generate a few minables only.
		totalCount = 1;
		uniform_real_distribution<> randEnergy(50., 100.);
		totalEnergy = randEnergy(gen) * .01;
	}

	double meanEnergy = totalEnergy / totalCount;
	totalCount /= 4;

	const unordered_map<const char *, double> probabilities = {
        { "aluminum", 12 },
        { "copper", 8 },
        { "gold", 2 },
        { "iron", 13 },
        { "lead", 15 },
        { "neodymium", 3 },
        { "platinum", 1 },
        { "silicon", 2 },
        { "silver", 5 },
        { "titanium", 11 },
        { "tungsten", 6 },
        { "uranium", 4 },
	};

	unordered_map<const char *, int> choices;
	uniform_int_distribution<> rand100(0, 99);
	for(int i = 0; i < 3; ++i)
	{
		uniform_int_distribution<> randCount(0, totalCount);
		totalCount = randCount(gen);
		if(!totalCount)
			break;

		int choice = rand100(gen);
		for(const auto &pair : probabilities)
		{
			choice -= pair.second;
			if(choice < 0)
			{
				choices[pair.first] += totalCount;
				break;
			}
		}
	}

	for(const auto &pair : choices)
	{
		const double energy = uniform_int_distribution<>(1000, 2000)(gen) * .001 * meanEnergy;
		object->asteroids.emplace_back(editor.Universe().minables.Get(pair.first), pair.second, energy);
	}

	UpdateMain();
	SetDirty();
}



void SystemEditor::GenerateTrades()
{
	for(const auto &commodity : editor.Universe().trade.Commodities())
	{
		int average = 0;
		int size = 0;
		for(const auto &link : object->links)
		{
			auto it = link->trade.find(commodity.name);
			if(it != link->trade.end())
			{
				average += it->second.base;
				++size;
			}
		}
		if(size)
			average /= size;

		// This system doesn't have any neighbors with the given commodity so we generate
		// a random price for it.
		if(!average)
		{
			uniform_int_distribution<> rand(commodity.low, commodity.high);
			average = rand(gen);
		}
		const int maxDeviation = (commodity.high - commodity.low) / 8;
		uniform_int_distribution<> rand(
				max(commodity.low, average - maxDeviation),
				min(commodity.high, average + maxDeviation));
		object->trade[commodity.name].SetBase(rand(gen));
	}

	SetDirty();
}



const Sprite *SystemEditor::RandomStarSprite()
{
	const auto &stars = editor.Spriteset().StarSprites();
	uniform_int_distribution<> randStar(0, stars.size() - 1);
	return stars[randStar(gen)];
}



const Sprite *SystemEditor::RandomPlanetSprite(bool recalculate)
{
	const auto &planets = editor.Spriteset().PlanetSprites();
	uniform_int_distribution<> randPlanet(0, planets.size() - 1);
	return planets[randPlanet(gen)];
}



const Sprite *SystemEditor::RandomMoonSprite()
{
	const auto &moons = editor.Spriteset().MoonSprites();
	uniform_int_distribution<> randMoon(0, moons.size() - 1);
	return moons[randMoon(gen)];
}



const Sprite *SystemEditor::RandomGiantSprite()
{
	const auto &giants = editor.Spriteset().GiantSprites();
	uniform_int_distribution<> randGiant(0, giants.size() - 1);
	return giants[randGiant(gen)];
}



void SystemEditor::Delete(const System *system, bool safe)
{
	if(!safe && editor.BaseUniverse().systems.Has(system->name))
		return;

	auto oldLinks = system->links;
	for(auto &&link : oldLinks)
	{
		const_cast<System *>(link)->Unlink(const_cast<System *>(system));
		SetDirty(link);
	}
	for(auto &&stellar : system->Objects())
		if(stellar.planet)
			const_cast<Planet *>(stellar.planet)->RemoveSystem(system);

	auto oldNeighbors = system->VisibleNeighbors();
	editor.GetPlugin().Remove(system);
	editor.Universe().systems.Erase(system->name);

	auto newSystem = oldLinks.empty() ?
		oldNeighbors.empty() ? nullptr : *oldNeighbors.begin()
		: *oldLinks.begin();
	editor.MapPanel()->Select(newSystem);
	editor.SystemViewPanel()->Select(newSystem);
	UpdateMap();
}
