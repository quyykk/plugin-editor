// SPDX-License-Identifier: GPL-3.0

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

	if(ImGui::Button(arenaPtr->paused ? "Unpause" : "Pause"))
		arenaPtr->paused = !arenaPtr->paused;
	ImGui::SameLine();
	if(ImGui::Button("Clear Ships"))
		arenaPtr->Execute([arenaPtr]
			{
				arenaPtr->engine.ships.clear();
			});

	ImGui::Spacing();

	// Ship spawning.
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
		arenaPtr->Execute([this, amount] {
			for(int i = 0; i < amount; ++i)
				PlaceShip(*ship, *gov);
		});
	if(!ship || !gov)
		ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::Text("x%d", amount);

	ImGui::Spacing();

	// Fleet spawning.
	static Fleet *fleet;
	string fleetName;
	if(fleet)
		fleetName = fleet->Name();
	ImGui::InputCombo("fleet", &fleetName, &fleet, editor.Universe().fleets);

	static Government *fleetgov;
	string fleetgovName;
	if(fleetgov)
		fleetgovName = fleetgov->TrueName();
	ImGui::InputCombo("government##fleet", &fleetgovName, &fleetgov, editor.Universe().governments);

	amount = 1;
	if(ImGui::GetIO().KeyShift)
		amount *= 5;
	if(ImGui::GetIO().KeyCtrl)
		amount *= 20;
	if(ImGui::GetIO().KeyAlt)
		amount *= 500;

	if(!fleet || !fleetgov)
		ImGui::BeginDisabled();
	if(ImGui::Button("Spawn##fleet"))
		arenaPtr->Execute([this, amount] {
			for(int i = 0; i < amount; ++i)
				for(const auto &ship : fleet->variants.Get().Ships())
					PlaceShip(*ship, *fleetgov);
		});
	if(!fleet || !fleetgov)
		ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::Text("x%d", amount);
	ImGui::End();
}



void ArenaControl::PlaceShip(const Ship &ship, const Government &gov) const
{
	auto newShip = make_shared<Ship>(ship);
	newShip->SetName(ship.TrueName());
	newShip->SetSystem(systemEditor.Selected());
	newShip->SetGovernment(&gov);
	newShip->SetPersonality(Personality::STAYING | Personality::UNINTERESTED);

	Fleet::Place(*systemEditor.Selected(), *newShip);
	arena.lock()->engine.Place(newShip);
}
