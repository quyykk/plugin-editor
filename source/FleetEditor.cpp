// SPDX-License-Identifier: GPL-3.0

#include "FleetEditor.h"

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



namespace
{
	std::unordered_map<int, const char *> PersonalityToString = {
		{ Personality::PACIFIST, "pacifist" },
		{ Personality::FORBEARING, "forebearing" },
		{ Personality::TIMID, "timid" },
		{ Personality::DISABLES, "disables" },
		{ Personality::PLUNDERS, "plunders" },
		{ Personality::HEROIC, "heroic" },
		{ Personality::STAYING, "staying" },
		{ Personality::ENTERING, "entering" },
		{ Personality::NEMESIS, "nemesis" },
		{ Personality::SURVEILLANCE, "surveillance" },
		{ Personality::UNINTERESTED, "uninterested" },
		{ Personality::WAITING, "waiting" },
		{ Personality::DERELICT, "derelict" },
		{ Personality::FLEEING, "fleeing" },
		{ Personality::ESCORT, "escort" },
		{ Personality::FRUGAL, "frugal" },
		{ Personality::COWARD, "coward" },
		{ Personality::VINDICTIVE, "vindictive" },
		{ Personality::SWARMING, "swarming" },
		{ Personality::UNCONSTRAINED, "unconstrained" },
		{ Personality::MINING, "mining" },
		{ Personality::HARVESTS, "harvests" },
		{ Personality::APPEASING, "appeasing" },
		{ Personality::MUTE, "mute" },
		{ Personality::OPPORTUNISTIC, "opportunistic" },
		{ Personality::TARGET, "target" },
		{ Personality::MARKED, "marked" },
		{ Personality::LAUNCHING, "launching" },
	};
}



FleetEditor::FleetEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Fleet>(editor, show)
{
}



void FleetEditor::Render()
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Fleet Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	bool showNewFleet = false;
	bool showRenameFleet = false;
	bool showCloneFleet = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Fleet"))
		{
			const bool alreadyDefined = object && !editor.BaseUniverse().fleets.Has(object->fleetName);
			ImGui::MenuItem("New", nullptr, &showNewFleet);
			ImGui::MenuItem("Rename", nullptr, &showRenameFleet, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneFleet, object);
			if(ImGui::MenuItem("Reset", nullptr, false, alreadyDefined && editor.GetPlugin().Has(object)))
			{
				editor.GetPlugin().Remove(object);
				*object = *editor.BaseUniverse().fleets.Get(object->fleetName);
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				editor.GetPlugin().Remove(object);
				editor.Universe().fleets.Erase(object->fleetName);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewFleet)
		ImGui::OpenPopup("New Fleet");
	if(showRenameFleet)
		ImGui::OpenPopup("Rename Fleet");
	if(showCloneFleet)
		ImGui::OpenPopup("Clone Fleet");
	ImGui::BeginSimpleNewModal("New Fleet", [this](const string &name)
			{
				if(editor.Universe().fleets.Find(name))
					return;

				auto *newFleet = editor.Universe().fleets.Get(name);
				newFleet->fleetName = name;
				object = newFleet;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Fleet", [this](const string &name)
			{
				if(editor.Universe().fleets.Find(name))
					return;

				editor.Universe().fleets.Rename(object->fleetName, name);
				object->fleetName = name;
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Fleet", [this](const string &name)
			{
				if(editor.Universe().fleets.Find(name))
					return;

				auto *clone = editor.Universe().fleets.Get(name);
				*clone = *object;
				object = clone;

				object->fleetName = name;
				SetDirty();
			});
	if(ImGui::InputCombo("fleet", &searchBox, &object, editor.Universe().fleets))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::PushID(object);
	if(object)
		RenderFleet();
	ImGui::PopID();
	ImGui::End();
}



void FleetEditor::RenderFleet()
{
	ImGui::Text("name: %s", object->fleetName.c_str());

	static Government *selected;
	string govName = object->government ? object->government->TrueName() : "";
	if(ImGui::InputCombo("government", &govName, &selected, editor.Universe().governments))
	{
		object->government = selected;
		SetDirty();
	}

	static Phrase *selectedPhrase;
	string names = object->names ? object->names->Name() : "";
	if(ImGui::InputCombo("names", &names, &selectedPhrase, editor.Universe().phrases))
	{
		object->names = selectedPhrase;
		SetDirty();
	}
	string fighterNames = object->fighterNames ? object->fighterNames->Name() : "";
	if(ImGui::InputCombo("fighters", &fighterNames, &selectedPhrase, editor.Universe().phrases))
	{
		object->fighterNames = selectedPhrase;
		SetDirty();
	}

	if(ImGui::InputInt("cargo", &object->cargo))
		SetDirty();
	if(ImGui::TreeNode("commodities"))
	{
		for(const auto &commodity : editor.Universe().trade.Commodities())
		{
			auto it = find(object->commodities.begin(), object->commodities.end(), commodity.name);
			bool has = it != object->commodities.end();
			if(ImGui::Checkbox(commodity.name.c_str(), &has))
			{
				if(!has)
					object->commodities.erase(it);
				else
					object->commodities.push_back(commodity.name);
				SetDirty();
			}
		}
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("outfitters"))
	{
		int index = 0;
		const Sale<Outfit> *toAdd = nullptr;
		const Sale<Outfit> *toRemove = nullptr;
		for(auto it = object->outfitters.begin(); it != object->outfitters.end(); ++it)
		{
			ImGui::PushID(index++);
			static Sale<Outfit> *selected;
			string name = (*it)->name;
			if(ImGui::InputCombo("##outfitter", &name, &selected, editor.Universe().outfitSales))
			{
				if(selected)
					toAdd = selected;
				toRemove = *it;
				SetDirty();
			}
			ImGui::PopID();
		}
		if(toAdd)
			object->outfitters.insert(toAdd);
		if(toRemove)
			object->outfitters.erase(toRemove);
		static Sale<Outfit> *selected;
		string name;
		ImGui::Spacing();
		if(ImGui::InputCombo("add outfitter", &name, &selected, editor.Universe().outfitSales))
			if(selected)
			{
				object->outfitters.insert(selected);
				SetDirty();
			}
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("personality"))
	{
		if(ImGui::InputDoubleEx("confusion", &object->personality.confusionMultiplier))
			SetDirty();
		bool flag = object->personality.flags & Personality::PACIFIST;
		if(ImGui::Checkbox("pacifist", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::PACIFIST;
			else
				object->personality.flags &= ~Personality::PACIFIST;
			SetDirty();
		}
		flag = object->personality.flags & Personality::FORBEARING;
		if(ImGui::Checkbox("forbearing", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::FORBEARING;
			else
				object->personality.flags &= ~Personality::FORBEARING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::TIMID;
		if(ImGui::Checkbox("timid", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::TIMID;
			else
				object->personality.flags &= ~Personality::TIMID;
			SetDirty();
		}
		flag = object->personality.flags & Personality::DISABLES;
		if(ImGui::Checkbox("disables", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::DISABLES;
			else
				object->personality.flags &= ~Personality::DISABLES;
			SetDirty();
		}
		flag = object->personality.flags & Personality::PLUNDERS;
		if(ImGui::Checkbox("plunders", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::PLUNDERS;
			else
				object->personality.flags &= ~Personality::PLUNDERS;
			SetDirty();
		}
		flag = object->personality.flags & Personality::HEROIC;
		if(ImGui::Checkbox("heroic", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::HEROIC;
			else
				object->personality.flags &= ~Personality::HEROIC;
			SetDirty();
		}
		flag = object->personality.flags & Personality::STAYING;
		if(ImGui::Checkbox("staying", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::STAYING;
			else
				object->personality.flags &= ~Personality::STAYING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::ENTERING;
		if(ImGui::Checkbox("entering", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::ENTERING;
			else
				object->personality.flags &= ~Personality::ENTERING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::NEMESIS;
		if(ImGui::Checkbox("nemesis", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::NEMESIS;
			else
				object->personality.flags &= ~Personality::NEMESIS;
			SetDirty();
		}
		flag = object->personality.flags & Personality::SURVEILLANCE;
		if(ImGui::Checkbox("surveillance", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::SURVEILLANCE;
			else
				object->personality.flags &= ~Personality::SURVEILLANCE;
			SetDirty();
		}
		flag = object->personality.flags & Personality::UNINTERESTED;
		if(ImGui::Checkbox("uninterested", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::UNINTERESTED;
			else
				object->personality.flags &= ~Personality::UNINTERESTED;
			SetDirty();
		}
		flag = object->personality.flags & Personality::WAITING;
		if(ImGui::Checkbox("waiting", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::WAITING;
			else
				object->personality.flags &= ~Personality::WAITING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::DERELICT;
		if(ImGui::Checkbox("derelict", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::DERELICT;
			else
				object->personality.flags &= ~Personality::DERELICT;
			SetDirty();
		}
		flag = object->personality.flags & Personality::FLEEING;
		if(ImGui::Checkbox("fleeing", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::FLEEING;
			else
				object->personality.flags &= ~Personality::FLEEING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::ESCORT;
		if(ImGui::Checkbox("escort", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::ESCORT;
			else
				object->personality.flags &= ~Personality::ESCORT;
			SetDirty();
		}
		flag = object->personality.flags & Personality::FRUGAL;
		if(ImGui::Checkbox("frugal", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::FRUGAL;
			else
				object->personality.flags &= ~Personality::FRUGAL;
			SetDirty();
		}
		flag = object->personality.flags & Personality::COWARD;
		if(ImGui::Checkbox("coward", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::COWARD;
			else
				object->personality.flags &= ~Personality::COWARD;
			SetDirty();
		}
		flag = object->personality.flags & Personality::VINDICTIVE;
		if(ImGui::Checkbox("vindictive", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::VINDICTIVE;
			else
				object->personality.flags &= ~Personality::VINDICTIVE;
			SetDirty();
		}
		flag = object->personality.flags & Personality::SWARMING;
		if(ImGui::Checkbox("swarming", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::SWARMING;
			else
				object->personality.flags &= ~Personality::SWARMING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::UNCONSTRAINED;
		if(ImGui::Checkbox("unconstrained", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::UNCONSTRAINED;
			else
				object->personality.flags &= ~Personality::UNCONSTRAINED;
			SetDirty();
		}
		flag = object->personality.flags & Personality::MINING;
		if(ImGui::Checkbox("mining", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::MINING;
			else
				object->personality.flags &= ~Personality::MINING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::HARVESTS;
		if(ImGui::Checkbox("harvests", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::HARVESTS;
			else
				object->personality.flags &= ~Personality::HARVESTS;
			SetDirty();
		}
		flag = object->personality.flags & Personality::APPEASING;
		if(ImGui::Checkbox("appeasing", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::APPEASING;
			else
				object->personality.flags &= ~Personality::APPEASING;
			SetDirty();
		}
		flag = object->personality.flags & Personality::MUTE;
		if(ImGui::Checkbox("mute", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::MUTE;
			else
				object->personality.flags &= ~Personality::MUTE;
			SetDirty();
		}
		flag = object->personality.flags & Personality::OPPORTUNISTIC;
		if(ImGui::Checkbox("opportunistic", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::OPPORTUNISTIC;
			else
				object->personality.flags &= ~Personality::OPPORTUNISTIC;
			SetDirty();
		}
		flag = object->personality.flags & Personality::TARGET;
		if(ImGui::Checkbox("target", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::TARGET;
			else
				object->personality.flags &= ~Personality::TARGET;
			SetDirty();
		}
		flag = object->personality.flags & Personality::MARKED;
		if(ImGui::Checkbox("marked", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::MARKED;
			else
				object->personality.flags &= ~Personality::MARKED;
			SetDirty();
		}
		flag = object->personality.flags & Personality::LAUNCHING;
		if(ImGui::Checkbox("launching", &flag))
		{
			if(flag)
				object->personality.flags |= Personality::LAUNCHING;
			else
				object->personality.flags &= ~Personality::LAUNCHING;
			SetDirty();
		}
		ImGui::TreePop();
	}

	bool variantsOpen = ImGui::TreeNode("variants");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Variant"))
		{
			Variant var;
			object->variants.emplace_back(1, move(var));
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(variantsOpen)
	{
		int index = 0;
		auto found = object->variants.end();
		for(auto it = object->variants.begin(); it != object->variants.end(); ++it)
		{
			ImGui::PushID(index++);
			bool open = ImGui::TreeNode("variant", "variant: %d", it->item.weight);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
				{
					found = it;
					SetDirty();
				}
				ImGui::EndPopup();
			}
			if(open)
			{
				if(ImGui::InputInt("weight", &it->item.weight))
					SetDirty();
				bool shipsOpen = ImGui::TreeNode("ships");
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Add Ship"))
					{
						it->item.ships.push_back(nullptr);
						SetDirty();
					}
					ImGui::EndPopup();
				}
				if(shipsOpen)
				{
					int modify = 0;
					auto modifyJt = it->item.ships.end();
					int shipIndex = 0;
					for(auto jt = it->item.ships.begin(); jt != it->item.ships.end(); ++jt)
					{
						ImGui::PushID(shipIndex++);
						auto first = jt;
						int count = 1;
						while(jt + 1 != it->item.ships.end() && *(jt + 1) == *jt)
						{
							++count;
							++jt;
						}
						string shipName = *first ? (*first)->VariantName() : "";
						bool shipOpen = ImGui::TreeNode("ship", "ship: %s %d", shipName.c_str(), count);
						if(ImGui::BeginPopupContextItem())
						{
							if(ImGui::Selectable("Remove"))
							{
								modifyJt = first;
								modify = -count;
								SetDirty();
							}
							ImGui::EndPopup();
						}
						if(shipOpen)
						{
							static Ship *ship = nullptr;
							if(ImGui::InputCombo("ship##input", &shipName, &ship, editor.Universe().ships))
								if(!shipName.empty())
								{
									*first = ship;
									ship = nullptr;
									SetDirty();
								}
							int oldCount = count;
							if(ImGui::InputInt("count", &count))
							{
								modify = count - oldCount;
								modifyJt = first;
								SetDirty();
							}
							ImGui::TreePop();
						}
						ImGui::PopID();
					}
					if(modifyJt != it->item.ships.end())
					{
						if(modify > 0)
							it->item.ships.insert(modifyJt, modify, *modifyJt);
						else if(modify < 0)
							it->item.ships.erase(modifyJt, modifyJt + (-modify));
						SetDirty();
					}
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
}



void FleetEditor::WriteToFile(DataWriter &writer, const Fleet *fleet) const
{
	const auto *diff = editor.BaseUniverse().fleets.Has(fleet->Name())
		? editor.BaseUniverse().fleets.Get(fleet->Name())
		: nullptr;

	writer.Write("fleet", fleet->Name());
	writer.BeginChild();
	if(!diff || fleet->government != diff->government)
		if(fleet->government)
			writer.Write("government", fleet->government->TrueName());
	if(!diff || fleet->names != diff->names)
		if(fleet->names)
			writer.Write("names", fleet->names->Name());
	if(!diff || fleet->fighterNames != diff->fighterNames)
		if(fleet->fighterNames)
			writer.Write("fighters", fleet->fighterNames->Name());
	if(!diff || fleet->cargo != diff->cargo)
		if(fleet->cargo != 3 || diff)
			writer.Write("cargo", fleet->cargo);
	if(!diff || fleet->commodities != fleet->commodities)
		if(!fleet->commodities.empty())
		{
			writer.WriteToken("commodities");
			for(const auto &commodity : fleet->commodities)
				writer.WriteToken(commodity);
			writer.Write();
		}
	if(!diff || fleet->outfitters != diff->outfitters)
		if(!fleet->outfitters.empty())
		{
			writer.WriteToken("outfitters");
			for(const auto &outfitter : fleet->outfitters)
				writer.WriteToken(outfitter->name);
			writer.Write();
		}
	if(!diff || fleet->personality.confusionMultiplier != diff->personality.confusionMultiplier
			|| fleet->personality.flags != diff->personality.flags)
		if(fleet->personality.confusionMultiplier || fleet->personality.flags || diff)
		{
			bool clearPersonality = diff && ((fleet->personality.flags ^ diff->personality.flags) & fleet->personality.flags);
			if(clearPersonality)
				writer.Write("remove", "personality");
			else
				writer.Write("personality");
			writer.BeginChild();
			if((!diff && fleet->personality.confusionMultiplier != 10.)
					|| (diff && fleet->personality.confusionMultiplier != diff->personality.confusionMultiplier))
				writer.Write("confusion", fleet->personality.confusionMultiplier);

			auto writeAll = [&writer](int flags, const char *opt = nullptr)
			{
				for(int i = 1; i <= (1 << 27); i <<= 1)
					if(auto personality = PersonalityToString[flags & i])
					{
						if(opt)
							writer.WriteToken(opt);
						writer.WriteToken(personality);
					}
				writer.Write();
			};

			if(!diff && fleet->personality.flags)
				writeAll(fleet->personality.flags);
			else if(diff)
			{
				int toAdd = (fleet->personality.flags ^ diff->personality.flags) & fleet->personality.flags;
				int toRemove = (fleet->personality.flags ^ diff->personality.flags) & diff->personality.flags;
				if(toRemove == diff->personality.flags && !toRemove)
				{
					if(!toAdd)
						writer.Write("remove", "personality");
					else
						writeAll(toAdd);
				}
				else
				{
					if(toAdd)
						writeAll(toAdd, "add");
					if(toRemove)
						writeAll(toRemove, "remove");
				}
			}
			writer.EndChild();
		}

	if(!diff || fleet->variants != diff->variants)
	{
		auto writeAll = [&writer](const WeightedList<Variant> &list, const char *opt = nullptr)
		{
			for(const auto &variant : list)
			{
				if(opt)
					writer.WriteToken(opt);
				writer.WriteToken("variant");
				if(variant.weight > 1)
					writer.WriteToken(variant.weight);
				writer.Write();
				writer.BeginChild();
				for(auto it = variant.item.ships.begin(); it != variant.item.ships.end(); )
				{
					auto copy = it;
					int count = 1;
					while(copy + 1 != variant.item.ships.end() && *(copy + 1) == *copy)
					{
						++copy;
						++count;
					}

					// Skip null ships.
					if(!*it)
						continue;

					writer.WriteToken((*it)->VariantName());
					if(count > 1)
						writer.WriteToken(count);
					writer.Write();
					it += count;
				}
				writer.EndChild();
			}
		};
		if(!diff)
			writeAll(fleet->variants);
		else
		{
			WeightedList<Variant> toAdd;
			auto toRemove = toAdd;

			for(auto &&it : fleet->variants)
				if(!Count(diff->variants, it))
					Insert(toAdd, it);
			for(auto &&it : diff->variants)
				if(!Count(fleet->variants, it))
					Insert(toRemove, it);

			if(!toAdd.empty() || !toRemove.empty())
			{
				if(toRemove.size() == diff->variants.size() && !diff->variants.empty())
				{
					if(fleet->variants.empty())
						writeAll(diff->variants, "remove");
					else
						writeAll(toAdd);
				}
				else
				{
					if(!toAdd.empty())
						writeAll(toAdd, "add");
					if(!toRemove.empty())
						writeAll(toRemove, "remove");
				}
			}
		}
	}
	writer.EndChild();
}
