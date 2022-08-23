/* GovernmentEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "GovernmentEditor.h"

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



GovernmentEditor::GovernmentEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Government>(editor, show)
{
}



void GovernmentEditor::Render()
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Government Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	bool showNewGovernment = false;
	bool showRenameGovernment = false;
	bool showCloneGovernment = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Government"))
		{
			const bool alreadyDefined = object && !editor.BaseUniverse().governments.Has(object->TrueName());
			ImGui::MenuItem("New", nullptr, &showNewGovernment);
			ImGui::MenuItem("Rename", nullptr, &showRenameGovernment, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneGovernment, object);
			if(ImGui::MenuItem("Reset", nullptr, false, alreadyDefined && editor.GetPlugin().Has(object)))
			{
				editor.GetPlugin().Remove(object);
				*object = *editor.BaseUniverse().governments.Get(object->TrueName());
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				editor.GetPlugin().Remove(object);
				editor.Universe().governments.Erase(object->TrueName());
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewGovernment)
		ImGui::OpenPopup("New Government");
	if(showRenameGovernment)
		ImGui::OpenPopup("Rename Government");
	if(showCloneGovernment)
		ImGui::OpenPopup("Clone Government");
	ImGui::BeginSimpleNewModal("New Government", [this](const string &name)
			{
				if(editor.Universe().governments.Find(name))
					return;

				auto *newGov = editor.Universe().governments.Get(name);
				newGov->name = name;
				newGov->displayName = name;
				object = newGov;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Government", [this](const string &name)
			{
				if(editor.Universe().governments.Find(name))
					return;

				editor.Universe().governments.Rename(object->TrueName(), name);
				object->name = name;
				object->displayName = name;
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Government", [this](const string &name)
			{
				if(editor.Universe().governments.Find(name))
					return;

				auto *clone = editor.Universe().governments.Get(name);
				*clone = *object;
				object = clone;

				object->name = name;
				object->displayName = name;
				SetDirty();
			});

	if(ImGui::InputCombo("government", &searchBox, &object, editor.Universe().governments))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::PushID(object);
	if(object)
		RenderGovernment();
	ImGui::PopID();
	ImGui::End();
}



void GovernmentEditor::RenderGovernment()
{
	ImGui::Text("name: %s", object->name.c_str());
	if(ImGui::InputText("display name", &object->displayName))
		SetDirty();
	if(ImGui::InputSwizzle("swizzle", &object->swizzle))
		SetDirty();
	float color[3] = {};
	color[0] = object->color.Get()[0];
	color[1] = object->color.Get()[1];
	color[2] = object->color.Get()[2];
	if(ImGui::ColorEdit3("color", color))
	{
		object->color = Color(color[0], color[1], color[2]);
		SetDirty();
	}
	if(ImGui::InputDoubleEx("player reputation", &object->initialPlayerReputation))
		SetDirty();
	if(ImGui::InputDoubleEx("crew attack", &object->crewAttack))
	{
		object->crewAttack = max(0., object->crewAttack);
		SetDirty();
	}
	if(ImGui::InputDoubleEx("crew defense", &object->crewDefense))
	{
		object->crewDefense = max(0., object->crewDefense);
		SetDirty();
	}

	if(ImGui::TreeNode("attitude towards"))
	{
		auto toRemove = object->attitudeToward.end();
		const Government *toAdd = nullptr;
		int index = 0;
		for(auto it = object->attitudeToward.begin(); it != object->attitudeToward.end(); ++it)
		{
			ImGui::PushID(index++);
			string govName = it->first ? it->first->TrueName() : "";
			bool open = ImGui::TreeNode("government", "%s %g", govName.c_str(), it->second);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
				{
					toRemove = it;
					SetDirty();
				}
				ImGui::EndPopup();
			}

			if(open)
			{
				static Government *selected;
				if(ImGui::InputCombo("government", &govName, &selected, editor.Universe().governments))
				{
					if(selected)
						toAdd = selected;
					toRemove = it;
					SetDirty();
				}

				if(ImGui::InputDoubleEx("value", &it->second))
					SetDirty();
				if(!it->second && ImGui::IsItemDeactivatedAfterEdit())
					toRemove = it;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(toRemove != object->attitudeToward.end())
		{
			double value = toRemove->second;
			object->attitudeToward.erase(toRemove);
			if(toAdd)
				object->attitudeToward[toAdd] = value;
		}

		ImGui::Spacing();
		static Government *selected;
		string name;
		if(ImGui::InputCombo("add government", &name, &selected, editor.Universe().governments))
			if(selected)
			{
				object->attitudeToward.emplace(selected, 0.);
				SetDirty();
			}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("penalty for"))
	{
		if(ImGui::InputDoubleEx("assist", &object->penaltyFor[ShipEvent::ASSIST]))
			SetDirty();
		if(ImGui::InputDoubleEx("disable", &object->penaltyFor[ShipEvent::DISABLE]))
			SetDirty();
		if(ImGui::InputDoubleEx("board", &object->penaltyFor[ShipEvent::BOARD]))
			SetDirty();
		if(ImGui::InputDoubleEx("capture", &object->penaltyFor[ShipEvent::CAPTURE]))
			SetDirty();
		if(ImGui::InputDoubleEx("destroy", &object->penaltyFor[ShipEvent::DESTROY]))
			SetDirty();
		if(ImGui::InputDoubleEx("atrocity", &object->penaltyFor[ShipEvent::ATROCITY]))
			SetDirty();
		ImGui::TreePop();
	}

	if(ImGui::InputDoubleEx("bribe", &object->bribe))
		SetDirty();
	if(ImGui::InputDoubleEx("fine", &object->fine))
		SetDirty();
	string deathSentenceName = object->deathSentence ? object->deathSentence->Name() : "";
	static Conversation *deathSentence;
	if(ImGui::InputCombo("death sentence", &deathSentenceName, &deathSentence, editor.Universe().conversations))
	{
		object->deathSentence = deathSentence;
		SetDirty();
	}
	string friendlyHail = object->friendlyHail ? object->friendlyHail->Name() : "";
	if(ImGui::InputCombo("friendly hail", &friendlyHail, &object->friendlyHail, editor.Universe().phrases))
		SetDirty();
	string friendlyDisabledHail = object->friendlyDisabledHail ? object->friendlyDisabledHail->Name() : "";
	if(ImGui::InputCombo("friendly disabled hail", &friendlyDisabledHail, &object->friendlyDisabledHail, editor.Universe().phrases))
	{
		if(!object->friendlyDisabledHail)
			object->friendlyDisabledHail = editor.Universe().phrases.Get("friendly disabled");
		SetDirty();
	}
	string hostileHail = object->hostileHail ? object->hostileHail->Name() : "";
	if(ImGui::InputCombo("hostile hail", &hostileHail, &object->hostileHail, editor.Universe().phrases))
		SetDirty();
	string hostileDisabledHail = object->hostileDisabledHail? object->hostileDisabledHail->Name() : "";
	if(ImGui::InputCombo("hostile disabled hail", &hostileDisabledHail, &object->hostileDisabledHail, editor.Universe().phrases))
	{
		if(!object->hostileDisabledHail)
			object->hostileDisabledHail = editor.Universe().phrases.Get("hostile disabled");
		SetDirty();
	}

	if(ImGui::InputText("language", &object->language))
		SetDirty();
	string raidName = object->raidFleet ? object->raidFleet->Name() : "";
	if(ImGui::InputCombo("raid fleet", &raidName, &object->raidFleet, editor.Universe().fleets))
		SetDirty();

	static string enforcements;
	if(ImGui::InputTextMultiline("enforces", &enforcements))
		SetDirty();
}



void GovernmentEditor::WriteToFile(DataWriter &writer, const Government *government) const
{
	const auto *diff = editor.BaseUniverse().governments.Has(government->TrueName())
		? editor.BaseUniverse().governments.Get(government->TrueName())
		: nullptr;

	writer.Write("government", government->TrueName());
	writer.BeginChild();
	if(!diff || government->displayName != diff->displayName)
		if(government->displayName != government->name || diff)
			writer.Write("display name", government->displayName);
	if(!diff || government->swizzle != diff->swizzle)
		if(government->swizzle || diff)
			writer.Write("swizzle", government->swizzle);
	if(!diff || government->color != diff->color)
		writer.Write("color", government->color.Get()[0], government->color.Get()[1], government->color.Get()[2]);
	if(!diff || government->initialPlayerReputation != diff->initialPlayerReputation)
		if(government->initialPlayerReputation || diff)
			writer.Write("player reputation", government->initialPlayerReputation);
	if(!diff || government->crewAttack != diff->crewAttack)
		if(government->crewAttack != 1. || diff)
			writer.Write("crew attack", government->crewAttack);
	if(!diff || government->crewDefense != diff->crewDefense)
		if(government->crewDefense != 2. || diff)
			writer.Write("crew defense", government->crewDefense);
	if(!diff || government->attitudeToward != diff->attitudeToward)
		if(!government->attitudeToward.empty())
		{
			writer.Write("attitude toward");
			writer.BeginChild();
			for(auto &&pair : government->attitudeToward)
				if(pair.second)
					writer.Write(pair.first->TrueName(), pair.second);
			writer.EndChild();
		}

	bool hasWrittenPenalty = false;
	auto writePenalty = [&hasWrittenPenalty, &writer]()
	{
		if(!hasWrittenPenalty)
		{
			writer.Write("penalty for");
			writer.BeginChild();
			hasWrittenPenalty = true;
		}
	};

	if(!diff || government->penaltyFor.at(ShipEvent::ASSIST) != diff->penaltyFor.at(ShipEvent::ASSIST))
		if(government->penaltyFor.at(ShipEvent::ASSIST) != -.1 || diff)
		{
			writePenalty();
			writer.Write("assist", government->penaltyFor.at(ShipEvent::ASSIST));
		}
	if(!diff || government->penaltyFor.at(ShipEvent::DISABLE) != diff->penaltyFor.at(ShipEvent::DISABLE))
		if(government->penaltyFor.at(ShipEvent::DISABLE) != .5 || diff)
		{
			writePenalty();
			writer.Write("disable", government->penaltyFor.at(ShipEvent::DISABLE));
		}
	if(!diff || government->penaltyFor.at(ShipEvent::BOARD) != diff->penaltyFor.at(ShipEvent::BOARD))
		if(government->penaltyFor.at(ShipEvent::BOARD) != .3 || diff)
		{
			writePenalty();
			writer.Write("board", government->penaltyFor.at(ShipEvent::BOARD));
		}
	if(!diff || government->penaltyFor.at(ShipEvent::CAPTURE) != diff->penaltyFor.at(ShipEvent::CAPTURE))
		if(government->penaltyFor.at(ShipEvent::CAPTURE) != 1. || diff)
		{
			writePenalty();
			writer.Write("capture", government->penaltyFor.at(ShipEvent::CAPTURE));
		}
	if(!diff || government->penaltyFor.at(ShipEvent::DESTROY) != diff->penaltyFor.at(ShipEvent::DESTROY))
		if(government->penaltyFor.at(ShipEvent::DESTROY) != 1. || diff)
		{
			writePenalty();
			writer.Write("destroy", government->penaltyFor.at(ShipEvent::DESTROY));
		}
	if(!diff || government->penaltyFor.at(ShipEvent::ATROCITY) != diff->penaltyFor.at(ShipEvent::ATROCITY))
		if(government->penaltyFor.at(ShipEvent::ATROCITY) != 10. || diff)
		{
			writePenalty();
			writer.Write("atrocity", government->penaltyFor.at(ShipEvent::ATROCITY));
		}
	if(hasWrittenPenalty)
		writer.EndChild();

	if(!diff || government->bribe != diff->bribe)
		if(government->bribe || diff)
			writer.Write("bribe", government->bribe);
	if(!diff || government->fine != diff->fine)
		if(government->fine != 1. || diff)
			writer.Write("fine", government->fine);
	if(!diff || government->deathSentence != diff->deathSentence)
		if(government->deathSentence)
			writer.Write("death sentence", government->deathSentence->Name());
	if(!diff || government->friendlyHail != diff->friendlyHail)
		if(government->friendlyHail || diff)
			writer.Write("friendly hail", government->friendlyHail->Name());
	if(!diff || government->friendlyDisabledHail != diff->friendlyDisabledHail)
		if((government->friendlyDisabledHail && government->friendlyDisabledHail->Name() != "friendly disabled") || diff)
		writer.Write("friendly disabled hail", government->friendlyDisabledHail->Name());
	if(!diff || government->hostileHail != diff->hostileHail)
		if(government->hostileHail || diff)
			writer.Write("hostile hail", government->hostileHail->Name());
	if(!diff || government->hostileDisabledHail != diff->hostileDisabledHail)
		if((government->hostileDisabledHail && government->hostileDisabledHail->Name() != "hostile disabled") || diff)
			writer.Write("hostile disabled hail", government->hostileDisabledHail->Name());
	if(!diff || government->language != diff->language)
		if(!government->language.empty() || diff)
			writer.Write("language", government->language);
	if(!diff || government->raidFleet != diff->raidFleet)
		if(government->raidFleet || diff)
			writer.Write("raid", government->raidFleet->Name());
	if(!diff || government->enforcementZones != diff->enforcementZones)
		if(!government->enforcementZones.empty() || diff)
		{
			writer.Write("enforces");
			for(auto &&filter : government->enforcementZones)
				filter.Save(writer);
		}
	writer.EndChild();
}
