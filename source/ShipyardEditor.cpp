/* ShipyardEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "ShipyardEditor.h"

#include "DataFile.h"
#include "DataNode.h"
#include "DataWriter.h"
#include "Dialog.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "Editor.h"
#include "Effect.h"
#include "Engine.h"
#include "Files.h"
#include "Government.h"
#include "Hazard.h"
#include "MainPanel.h"
#include "MapPanel.h"
#include "Minable.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Ship.h"
#include "ShipEvent.h"
#include "Sound.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"

#include <cassert>
#include <map>

using namespace std;



ShipyardEditor::ShipyardEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Sale<Ship>>(editor, show)
{
}



void ShipyardEditor::Render()
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Shipyard Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	bool showNewShipyard = false;
	bool showRenameShipyard = false;
	bool showCloneShipyard = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Shipyard"))
		{
			const bool alreadyDefined = object && !editor.BaseUniverse().shipSales.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewShipyard);
			ImGui::MenuItem("Rename", nullptr, &showRenameShipyard, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneShipyard, object);
			if(ImGui::MenuItem("Reset", nullptr, false, alreadyDefined && editor.GetPlugin().Has(object)))
			{
				editor.GetPlugin().Remove(object);
				*object = *editor.BaseUniverse().shipSales.Get(object->name);
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				editor.GetPlugin().Remove(object);
				editor.Universe().shipSales.Erase(object->name);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewShipyard)
		ImGui::OpenPopup("New Shipyard");
	if(showRenameShipyard)
		ImGui::OpenPopup("Rename Shipyard");
	if(showCloneShipyard)
		ImGui::OpenPopup("Clone Shipyard");
	ImGui::BeginSimpleNewModal("New Shipyard", [this](const string &name)
			{
				if(editor.Universe().shipSales.Find(name))
					return;

				auto *newShipyard = editor.Universe().shipSales.Get(name);
				newShipyard->name = name;
				object = newShipyard;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Shipyard", [this](const string &name)
			{
				if(editor.Universe().shipSales.Find(name))
					return;

				editor.Universe().shipSales.Rename(object->name, name);
				object->name = name;
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Shipyard", [this](const string &name)
			{
				if(editor.Universe().shipSales.Find(name))
					return;

				auto *clone = editor.Universe().shipSales.Get(name);
				*clone = *object;
				object = clone;

				object->name = name;
				SetDirty();
			});

	if(ImGui::InputCombo("shipyard", &searchBox, &object, editor.Universe().shipSales))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::PushID(object);
	if(object)
		RenderShipyard();
	ImGui::PopID();
	ImGui::End();
}



void ShipyardEditor::RenderShipyard()
{
	ImGui::Text("name: %s", object->name.c_str());
	int index = 0;
	const Ship *toAdd = nullptr;
	const Ship *toRemove = nullptr;
	for(auto it = object->begin(); it != object->end(); ++it)
	{
		ImGui::PushID(index++);
		string name = (*it)->TrueName();
		Ship *change = nullptr;
		if(ImGui::InputCombo("##ship", &name, &change, editor.Universe().ships))
		{
			if(name.empty())
			{
				toRemove = *it;
				SetDirty();
			}
			else
			{
				toAdd = change;
				toRemove = *it;
				SetDirty();
			}
		}
		ImGui::PopID();
	}
	if(toAdd)
		object->insert(toAdd);
	if(toRemove)
		object->erase(toRemove);

	static string shipName;
	static Ship *ship = nullptr;
	ImGui::Spacing();
	if(ImGui::InputCombo("add ship", &shipName, &ship, editor.Universe().ships))
		if(!shipName.empty())
		{
			object->insert(ship);
			SetDirty();
			shipName.clear();
		}
}



void ShipyardEditor::WriteToFile(DataWriter &writer, const Sale<Ship> *shipyard) const
{
	const auto *diff = editor.BaseUniverse().shipSales.Has(shipyard->name)
		? editor.BaseUniverse().shipSales.Get(shipyard->name)
		: nullptr;

	writer.Write("shipyard", shipyard->name);
	writer.BeginChild();
	if(diff)
		WriteSorted(diff->AsBase(), [](const Ship *lhs, const Ship *rhs) { return lhs->TrueName() < rhs->TrueName(); },
				[&writer, &shipyard](const Ship &ship)
				{
					if(!shipyard->Has(&ship))
						writer.Write("remove", ship.TrueName());
				});
	WriteSorted(shipyard->AsBase(), [](const Ship *lhs, const Ship *rhs) { return lhs->TrueName() < rhs->TrueName(); },
			[&writer, &diff](const Ship &ship)
			{
				if(!diff || !diff->Has(&ship))
					writer.Write(ship.TrueName());
			});
	writer.EndChild();
}
