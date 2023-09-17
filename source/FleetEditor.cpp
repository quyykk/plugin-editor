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
		{ Personality::HUNTING, "hunting" },
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
		{ Personality::MERCIFUL, "merciful" },
		{ Personality::TARGET, "target" },
		{ Personality::MARKED, "marked" },
		{ Personality::LAUNCHING, "launching" },
		{ Personality::LINGERING, "lingering" },
		{ Personality::DARING, "daring" },
		{ Personality::SECRETIVE, "secretive" },
		{ Personality::RAMMING, "ramming" },
		{ Personality::DECLOAKED, "decloaked" }
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

	if(ImGui::InputInt("cargo", &object->cargo.cargo))
		SetDirty();
	if(ImGui::TreeNode("commodities"))
	{
		for(const auto &commodity : editor.Universe().trade.Commodities())
		{
			auto it = find(object->cargo.commodities.begin(), object->cargo.commodities.end(), commodity.name);
			bool has = it != object->cargo.commodities.end();
			if(ImGui::Checkbox(commodity.name.c_str(), &has))
			{
				if(!has)
					object->cargo.commodities.erase(it);
				else
					object->cargo.commodities.push_back(commodity.name);
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
		for(auto it = object->cargo.outfitters.begin(); it != object->cargo.outfitters.end(); ++it)
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
			object->cargo.outfitters.insert(toAdd);
		if(toRemove)
			object->cargo.outfitters.erase(toRemove);
		static Sale<Outfit> *selected;
		string name;
		ImGui::Spacing();
		if(ImGui::InputCombo("add outfitter", &name, &selected, editor.Universe().outfitSales))
			if(selected)
			{
				object->cargo.outfitters.insert(selected);
				SetDirty();
			}
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("personality"))
	{
		if(ImGui::InputDoubleEx("confusion", &object->personality.confusionMultiplier))
			SetDirty();
		bool flag = object->personality.flags.test(Personality::PACIFIST);
		if(ImGui::Checkbox("pacifist", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::PACIFIST);
			else
				object->personality.flags.reset(Personality::PACIFIST);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::FORBEARING);
		if(ImGui::Checkbox("forbearing", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::FORBEARING);
			else
				object->personality.flags.reset(Personality::FORBEARING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::TIMID);
		if(ImGui::Checkbox("timid", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::TIMID);
			else
				object->personality.flags.reset(Personality::TIMID);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::DISABLES);
		if(ImGui::Checkbox("disables", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::DISABLES);
			else
				object->personality.flags.reset(Personality::DISABLES);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::PLUNDERS);
		if(ImGui::Checkbox("plunders", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::PLUNDERS);
			else
				object->personality.flags.reset(Personality::PLUNDERS);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::HUNTING);
		if(ImGui::Checkbox("hunting", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::HUNTING);
			else
				object->personality.flags.reset(Personality::HUNTING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::STAYING);
		if(ImGui::Checkbox("staying", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::STAYING);
			else
				object->personality.flags.reset(Personality::STAYING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::ENTERING);
		if(ImGui::Checkbox("entering", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::ENTERING);
			else
				object->personality.flags.reset(Personality::ENTERING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::NEMESIS);
		if(ImGui::Checkbox("nemesis", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::NEMESIS);
			else
				object->personality.flags.reset(Personality::NEMESIS);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::SURVEILLANCE);
		if(ImGui::Checkbox("surveillance", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::SURVEILLANCE);
			else
				object->personality.flags.reset(Personality::SURVEILLANCE);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::UNINTERESTED);
		if(ImGui::Checkbox("uninterested", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::UNINTERESTED);
			else
				object->personality.flags.reset(Personality::UNINTERESTED);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::WAITING);
		if(ImGui::Checkbox("waiting", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::WAITING);
			else
				object->personality.flags.reset(Personality::WAITING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::DERELICT);
		if(ImGui::Checkbox("derelict", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::DERELICT);
			else
				object->personality.flags.reset(Personality::DERELICT);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::FLEEING);
		if(ImGui::Checkbox("fleeing", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::FLEEING);
			else
				object->personality.flags.reset(Personality::FLEEING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::ESCORT);
		if(ImGui::Checkbox("escort", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::ESCORT);
			else
				object->personality.flags.reset(Personality::ESCORT);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::FRUGAL);
		if(ImGui::Checkbox("frugal", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::FRUGAL);
			else
				object->personality.flags.reset(Personality::FRUGAL);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::COWARD);
		if(ImGui::Checkbox("coward", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::COWARD);
			else
				object->personality.flags.reset(Personality::COWARD);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::VINDICTIVE);
		if(ImGui::Checkbox("vindictive", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::VINDICTIVE);
			else
				object->personality.flags.reset(Personality::VINDICTIVE);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::SWARMING);
		if(ImGui::Checkbox("swarming", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::SWARMING);
			else
				object->personality.flags.reset(Personality::SWARMING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::UNCONSTRAINED);
		if(ImGui::Checkbox("unconstrained", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::UNCONSTRAINED);
			else
				object->personality.flags.reset(Personality::UNCONSTRAINED);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::MINING);
		if(ImGui::Checkbox("mining", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::MINING);
			else
				object->personality.flags.reset(Personality::MINING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::HARVESTS);
		if(ImGui::Checkbox("harvests", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::HARVESTS);
			else
				object->personality.flags.reset(Personality::HARVESTS);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::APPEASING);
		if(ImGui::Checkbox("appeasing", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::APPEASING);
			else
				object->personality.flags.reset(Personality::APPEASING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::MUTE);
		if(ImGui::Checkbox("mute", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::MUTE);
			else
				object->personality.flags.reset(Personality::MUTE);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::OPPORTUNISTIC);
		if(ImGui::Checkbox("opportunistic", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::OPPORTUNISTIC);
			else
				object->personality.flags.reset(Personality::OPPORTUNISTIC);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::MERCIFUL);
		if(ImGui::Checkbox("merciful", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::MERCIFUL);
			else
				object->personality.flags.reset(Personality::MERCIFUL);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::TARGET);
		if(ImGui::Checkbox("target", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::TARGET);
			else
				object->personality.flags.reset(Personality::TARGET);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::MARKED);
		if(ImGui::Checkbox("marked", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::MARKED);
			else
				object->personality.flags.reset(Personality::MARKED);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::LAUNCHING);
		if(ImGui::Checkbox("launching", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::LAUNCHING);
			else
				object->personality.flags.reset(Personality::LAUNCHING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::LINGERING);
		if(ImGui::Checkbox("lingering", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::LINGERING);
			else
				object->personality.flags.reset(Personality::LINGERING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::DARING);
		if(ImGui::Checkbox("daring", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::DARING);
			else
				object->personality.flags.reset(Personality::DARING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::SECRETIVE);
		if(ImGui::Checkbox("secretive", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::SECRETIVE);
			else
				object->personality.flags.reset(Personality::SECRETIVE);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::RAMMING);
		if(ImGui::Checkbox("ramming", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::RAMMING);
			else
				object->personality.flags.reset(Personality::RAMMING);
			SetDirty();
		}
		flag = object->personality.flags.test(Personality::DECLOAKED);
		if(ImGui::Checkbox("decloaked", &flag))
		{
			if(flag)
				object->personality.flags.set(Personality::DECLOAKED);
			else
				object->personality.flags.reset(Personality::DECLOAKED);
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
			object->variants.emplace_back(1, std::move(var));
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
	if(!diff || fleet->cargo.cargo != diff->cargo.cargo)
		if(fleet->cargo.cargo != 3 || diff)
			writer.Write("cargo", fleet->cargo.cargo);
	if(!diff || fleet->cargo.commodities != fleet->cargo.commodities)
		if(!fleet->cargo.commodities.empty())
		{
			writer.WriteToken("commodities");
			for(const auto &commodity : fleet->cargo.commodities)
				writer.WriteToken(commodity);
			writer.Write();
		}
	if(!diff || fleet->cargo.outfitters != diff->cargo.outfitters)
		if(!fleet->cargo.outfitters.empty())
		{
			writer.WriteToken("outfitters");
			for(const auto &outfitter : fleet->cargo.outfitters)
				writer.WriteToken(outfitter->name);
			writer.Write();
		}
	if(!diff || fleet->personality.confusionMultiplier != diff->personality.confusionMultiplier
			|| fleet->personality.flags != diff->personality.flags)
		if(fleet->personality.confusionMultiplier || fleet->personality.flags.any() || diff)
		{
			bool clearPersonality = diff && ((fleet->personality.flags ^ diff->personality.flags) & fleet->personality.flags).any();
			if(clearPersonality)
				writer.Write("remove", "personality");
			else
				writer.Write("personality");
			writer.BeginChild();
			if((!diff && fleet->personality.confusionMultiplier != 10.)
					|| (diff && fleet->personality.confusionMultiplier != diff->personality.confusionMultiplier))
				writer.Write("confusion", fleet->personality.confusionMultiplier);

			auto writeAll = [&writer](auto flags, const char *opt = nullptr)
			{
				for(auto i = 0ull; i < flags.size(); ++i)
					if(auto personality = PersonalityToString[flags.test(i)])
					{
						if(opt)
							writer.WriteToken(opt);
						writer.WriteToken(personality);
					}
				writer.Write();
			};

			if(!diff && fleet->personality.flags.any())
				writeAll(fleet->personality.flags);
			else if(diff)
			{
				auto toAdd = (fleet->personality.flags ^ diff->personality.flags) & fleet->personality.flags;
				auto toRemove = (fleet->personality.flags ^ diff->personality.flags) & diff->personality.flags;
				if(toRemove == diff->personality.flags && toRemove.none())
				{
					if(toAdd.none())
						writer.Write("remove", "personality");
					else
						writeAll(toAdd);
				}
				else
				{
					if(toAdd.any())
						writeAll(toAdd, "add");
					if(toRemove.any())
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
