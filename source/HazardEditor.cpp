// SPDX-License-Identifier: GPL-3.0

#include "HazardEditor.h"

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
#include "Sound.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"

#include <cassert>
#include <map>

using namespace std;



HazardEditor::HazardEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Hazard>(editor, show)
{
}



void HazardEditor::Render()
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Hazard Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	bool showNewHazard = false;
	bool showRenameHazard = false;
	bool showCloneHazard = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Hazard"))
		{
			const bool alreadyDefined = object && !editor.BaseUniverse().hazards.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewHazard);
			ImGui::MenuItem("Rename", nullptr, &showRenameHazard, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneHazard, object);
			if(ImGui::MenuItem("Reset", nullptr, false, alreadyDefined && editor.GetPlugin().Has(object)))
			{
				editor.GetPlugin().Remove(object);
				*object = *editor.BaseUniverse().hazards.Get(object->name);
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				editor.GetPlugin().Remove(object);
				editor.Universe().hazards.Erase(object->name);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewHazard)
		ImGui::OpenPopup("New Hazard");
	if(showRenameHazard)
		ImGui::OpenPopup("Rename Hazard");
	if(showCloneHazard)
		ImGui::OpenPopup("Clone Hazard");
	ImGui::BeginSimpleNewModal("New Hazard", [this](const string &name)
			{
				if(GameData::Hazards().Find(name))
					return;

				auto *newHazard = editor.Universe().hazards.Get(name);
				newHazard->name = name;
				object = newHazard;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Hazard", [this](const string &name)
			{
				if(GameData::Hazards().Find(name))
					return;

				editor.Universe().hazards.Rename(object->name, name);
				object->name = name;
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Hazard", [this](const string &name)
			{
				if(GameData::Hazards().Find(name))
					return;

				auto *clone = editor.Universe().hazards.Get(name);
				*clone = *object;
				object = clone;

				object->name = name;
				SetDirty();
			});

	if(ImGui::InputCombo("hazard", &searchBox, &object, editor.Universe().hazards))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::PushID(object);
	if(object)
		RenderHazard();
	ImGui::PopID();
	ImGui::End();
}



void HazardEditor::RenderHazard()
{
	ImGui::Text("name: %s", object->name.c_str());
	bool constantStrength = !object->deviates;
	if(ImGui::Checkbox("constant strength", &constantStrength))
	{
		object->deviates = !constantStrength;
		SetDirty();
	}
	if(ImGui::InputInt("period", &object->period))
	{
		object->period = max(1, object->period);
		SetDirty();
	}
	int durations[2] = {object->minDuration, object->maxDuration};
	if(ImGui::InputInt2("duration", durations))
	{
		object->minDuration = max(0, durations[0]);
		object->maxDuration = max(object->minDuration, durations[1]);
		SetDirty();
	}
	double strengths[2] = {object->minStrength, object->maxStrength};
	if(ImGui::InputDouble2Ex("strength", strengths))
	{
		object->minStrength = max(0., strengths[0]);
		object->maxStrength = max(object->minStrength, strengths[1]);
		SetDirty();
	}
	double ranges[2] = {object->minRange, object->maxRange};
	if(ImGui::InputDouble2Ex("range", ranges))
	{
		object->minRange = max(0., ranges[0]);
		object->maxRange = max(object->minRange, ranges[1]);
		SetDirty();
	}

	RenderEffect("environmental effects", object->environmentalEffects);

	if(ImGui::TreeNode("weapon"))
	{
		if(ImGui::InputDoubleEx("shield damage", &object->damage[Weapon::SHIELD_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("hull damage", &object->damage[Weapon::HULL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("fuel damage", &object->damage[Weapon::FUEL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("heat damage", &object->damage[Weapon::HEAT_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("energy damage", &object->damage[Weapon::ENERGY_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("ion damage", &object->damage[Weapon::ION_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("disruption damage", &object->damage[Weapon::DISRUPTION_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("slowing damage", &object->damage[Weapon::SLOWING_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative shield damage", &object->damage[Weapon::RELATIVE_SHIELD_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative hull damage", &object->damage[Weapon::RELATIVE_HULL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative fuel damage", &object->damage[Weapon::RELATIVE_FUEL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative heat damage", &object->damage[Weapon::RELATIVE_HEAT_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative energy damage", &object->damage[Weapon::RELATIVE_ENERGY_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("piecing", &object->piercing))
		{
			object->piercing = max(0., object->piercing);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("hit force", &object->damage[Weapon::HIT_FORCE]))
			SetDirty();
		if(ImGui::Checkbox("gravitational", &object->isGravitational))
			SetDirty();
		if(ImGui::InputDoubleEx("blast radius", &object->blastRadius))
		{
			object->blastRadius = max(0., object->blastRadius);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("trigger radius", &object->triggerRadius))
		{
			object->triggerRadius = max(0., object->triggerRadius);
			SetDirty();
		}
		double dropoff[2] = {object->damageDropoffRange.first, object->damageDropoffRange.second};
		if(ImGui::InputDouble2Ex("damage dropoff", dropoff))
		{
			object->damageDropoffRange.first = max(0., dropoff[0]);
			object->damageDropoffRange.second = dropoff[1];
			object->hasDamageDropoff = true;
			SetDirty();
		}
		if(ImGui::InputDoubleEx("dropoff modifier", &object->damageDropoffModifier))
		{
			object->damageDropoffModifier = max(0., object->damageDropoffModifier);
			SetDirty();
		}
		RenderEffect("target effect", object->targetEffects);
		ImGui::TreePop();
	}
}



void HazardEditor::WriteToFile(DataWriter &writer, const Hazard *hazard) const
{
	const auto *diff = editor.BaseUniverse().hazards.Has(hazard->name)
		? editor.BaseUniverse().hazards.Get(hazard->name)
		: nullptr;

	writer.Write("hazard", hazard->Name());
	writer.BeginChild();

	if(!diff || hazard->deviates != diff->deviates)
		if(!hazard->deviates)
			writer.Write("constant strength");
	if(!diff || hazard->period != diff->period)
		if(hazard->period > 1 || diff)
			writer.Write("period", hazard->period);
	if(!diff || hazard->minDuration != diff->minDuration || hazard->maxDuration != diff->maxDuration)
		if(hazard->minDuration > 1 || hazard->maxDuration > 1 || diff)
		{
			writer.WriteToken("duration");
			writer.WriteToken(hazard->minDuration);
			if(hazard->maxDuration > 1 || (diff && hazard->maxDuration != diff->maxDuration))
				writer.WriteToken(hazard->maxDuration);
			writer.Write();
		}
	if(!diff || hazard->minStrength != diff->minStrength || hazard->maxStrength != diff->maxStrength)
		if(hazard->minStrength > 1. || hazard->maxStrength > 1. || diff)
		{
			writer.WriteToken("strength");
			writer.WriteToken(hazard->minStrength);
			if(hazard->maxStrength > 1. || (diff && hazard->maxStrength != diff->maxStrength))
				writer.WriteToken(hazard->maxStrength);
			writer.Write();
		}
	if(!diff || hazard->minRange != diff->minRange || hazard->maxRange != diff->maxRange)
		if(hazard->minRange > 0. || hazard->maxRange < 10000. || diff)
		{
			writer.WriteToken("range");
			if(hazard->minRange > 0. || (diff && hazard->minRange != diff->minRange))
				writer.WriteToken(hazard->minRange);
			writer.WriteToken(hazard->maxRange);
			writer.Write();
		}
	if(!diff || hazard->environmentalEffects != diff->environmentalEffects)
		for(const auto &pair : hazard->environmentalEffects)
			writer.Write("environmental effect", pair.first->Name(), pair.second);

	bool hasWrittenRoot = false;
	auto writeRoot = [&hasWrittenRoot, &writer]
	{
		if(!hasWrittenRoot)
		{
			writer.Write("weapon");
			writer.BeginChild();
			hasWrittenRoot = true;
		}
	};
	if(!diff || hazard->damage[Weapon::SHIELD_DAMAGE] != diff->damage[Weapon::SHIELD_DAMAGE])
		if(hazard->damage[Weapon::SHIELD_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("shield damage", hazard->damage[Weapon::SHIELD_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::HULL_DAMAGE] != diff->damage[Weapon::HULL_DAMAGE])
		if(hazard->damage[Weapon::HULL_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("hull damage", hazard->damage[Weapon::HULL_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::FUEL_DAMAGE] != diff->damage[Weapon::FUEL_DAMAGE])
		if(hazard->damage[Weapon::FUEL_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("fuel damage", hazard->damage[Weapon::FUEL_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::HEAT_DAMAGE] != diff->damage[Weapon::HEAT_DAMAGE])
		if(hazard->damage[Weapon::HEAT_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("heat damage", hazard->damage[Weapon::HEAT_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::ENERGY_DAMAGE] != diff->damage[Weapon::ENERGY_DAMAGE])
		if(hazard->damage[Weapon::ENERGY_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("energy damage", hazard->damage[Weapon::ENERGY_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::ION_DAMAGE] != diff->damage[Weapon::ION_DAMAGE])
		if(hazard->damage[Weapon::ION_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("ion damage", hazard->damage[Weapon::ION_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::DISRUPTION_DAMAGE] != diff->damage[Weapon::DISRUPTION_DAMAGE])
		if(hazard->damage[Weapon::DISRUPTION_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("disruption damage", hazard->damage[Weapon::DISRUPTION_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::SLOWING_DAMAGE] != diff->damage[Weapon::SLOWING_DAMAGE])
		if(hazard->damage[Weapon::SLOWING_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("slowing damage", hazard->damage[Weapon::SLOWING_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::RELATIVE_SHIELD_DAMAGE] != diff->damage[Weapon::RELATIVE_SHIELD_DAMAGE])
		if(hazard->damage[Weapon::RELATIVE_SHIELD_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("relative shield damage", hazard->damage[Weapon::RELATIVE_SHIELD_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::RELATIVE_HULL_DAMAGE] != diff->damage[Weapon::RELATIVE_HULL_DAMAGE])
		if(hazard->damage[Weapon::RELATIVE_HULL_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("relative hull damage", hazard->damage[Weapon::RELATIVE_HULL_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::RELATIVE_FUEL_DAMAGE] != diff->damage[Weapon::RELATIVE_FUEL_DAMAGE])
		if(hazard->damage[Weapon::RELATIVE_FUEL_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("relative fuel damage", hazard->damage[Weapon::RELATIVE_FUEL_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::RELATIVE_HEAT_DAMAGE] != diff->damage[Weapon::RELATIVE_HEAT_DAMAGE])
		if(hazard->damage[Weapon::RELATIVE_HEAT_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("relative heat damage", hazard->damage[Weapon::RELATIVE_HEAT_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::RELATIVE_ENERGY_DAMAGE] != diff->damage[Weapon::RELATIVE_ENERGY_DAMAGE])
		if(hazard->damage[Weapon::RELATIVE_ENERGY_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("relative energy damage", hazard->damage[Weapon::RELATIVE_ENERGY_DAMAGE]);
		}
	if(!diff || hazard->piercing != diff->piercing)
		if(hazard->piercing || diff)
		{
			writeRoot();
			writer.Write("piercing", hazard->piercing);
		}
	if(!diff || hazard->damage[Weapon::HIT_FORCE] != diff->damage[Weapon::HIT_FORCE])
		if(hazard->damage[Weapon::HIT_FORCE] || diff)
		{
			writeRoot();
			writer.Write("hit force", hazard->damage[Weapon::HIT_FORCE]);
		}
	if(!diff || hazard->isGravitational != diff->isGravitational)
		if(hazard->isGravitational || diff)
		{
			writeRoot();
			writer.Write("gravitational");
		}
	if(!diff || hazard->blastRadius != diff->blastRadius)
		if(hazard->blastRadius || diff)
		{
			writeRoot();
			writer.Write("blast radius", hazard->blastRadius);
		}
	if(!diff || hazard->damageDropoffRange != diff->damageDropoffRange)
		if(hazard->damageDropoffRange.first || hazard->damageDropoffRange.second || diff)
		{
			writeRoot();
			writer.Write("damage dropoff", hazard->damageDropoffRange.first, hazard->damageDropoffRange.second);
		}
	if(!diff || hazard->damageDropoffModifier != diff->damageDropoffModifier)
		if(hazard->damageDropoffModifier || diff)
		{
			writeRoot();
			writer.Write("dropoff modifier", hazard->damageDropoffModifier);
		}
	if(!diff || hazard->targetEffects != diff->targetEffects)
		if(!hazard->targetEffects.empty())
		{
			writeRoot();
			for(auto &&targetEffect : hazard->targetEffects)
				writer.Write("target effect", targetEffect.first->Name(), targetEffect.second);
		}
	if(hasWrittenRoot)
		writer.EndChild();
	writer.EndChild();
}
