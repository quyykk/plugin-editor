/* ArenaControl.cpp
Copyright (c) 2022 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "ArenaControl.h"

#include "Editor.h"
#include "Fleet.h"
#include "imgui_ex.h"
#include "Ship.h"

#include <cassert>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <map>

using namespace std;

namespace {
}


ArenaControl::ArenaControl(Editor &editor, SystemEditor &systemEditor)
	: editor(editor), systemEditor(systemEditor)
{}



void ArenaControl::SetArena(weak_ptr<ArenaPanel> ptr)
{
	arena = std::move(ptr);
}



void ArenaControl::Render(bool &show)
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Arena Control", &show))
	{
		ImGui::End();
		return;
	}

	auto arenaPtr = arena.lock();
	if(!arenaPtr)
		return ImGui::End();

	if(ImGui::Button("Clear Ships"))
		arenaPtr->Execute([arenaPtr]
			{
				arenaPtr->engine.ships.clear();
			});

	ImGui::Spacing();

	static Ship *ship;
	string shipName;
	if(ship)
		shipName = ship->VariantName();
	ImGui::InputCombo("ship", &shipName, &ship, editor.Universe().ships);

	static Government *gov;
	string govName;
	if(gov)
		govName = gov->TrueName();
	ImGui::InputCombo("government", &govName, &gov, editor.Universe().governments);

	int amount = 1;
	if(ImGui::GetIO().KeyShift)
		amount *= 5;
	if(ImGui::GetIO().KeyCtrl)
		amount *= 20;
	if(ImGui::GetIO().KeyAlt)
		amount *= 500;

	if(!ship || !gov)
		ImGui::BeginDisabled();
	if(ImGui::Button("Spawn"))
		arenaPtr->Execute([this, arenaPtr, amount] {
			for(int i = 0; i < amount; ++i)
			{
				auto newShip = make_shared<Ship>(*ship);
				newShip->SetName(ship->TrueName());
				newShip->SetSystem(systemEditor.Selected());
				newShip->SetGovernment(gov);
				newShip->SetPersonality(Personality::STAYING | Personality::UNINTERESTED);

				Fleet::Place(*systemEditor.Selected(), *newShip);
				arenaPtr->engine.Place(newShip);
			}
		});
	if(!ship || !gov)
		ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::Text("x%d", amount);

	ImGui::End();
}
