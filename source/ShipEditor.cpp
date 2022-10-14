// SPDX-License-Identifier: GPL-3.0

#include "ShipEditor.h"

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
#include "EsUuid.h"
#include "Engine.h"
#include "Files.h"
#include "Government.h"
#include "Hazard.h"
#include "MainPanel.h"
#include "MapPanel.h"
#include "Minable.h"
#include "OutfitterEditorPanel.h"
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



ShipEditor::ShipEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Ship>(editor, show)
{
}



void ShipEditor::Render()
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Ship Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	bool showNewShip = false;
	bool showRenameShip = false;
	bool showCloneModel = false;
	bool showCloneVariant = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Ship"))
		{
			const bool alreadyDefined = object && !editor.BaseUniverse().ships.Has(object->TrueName());
			ImGui::MenuItem("New", nullptr, &showNewShip);
			ImGui::MenuItem("Rename", nullptr, &showRenameShip, alreadyDefined);
			ImGui::MenuItem("Clone Model", nullptr, &showCloneModel, object);
			ImGui::MenuItem("Clone Variant", nullptr, &showCloneVariant, object);
			if(ImGui::MenuItem("Reset", nullptr, false, alreadyDefined && editor.GetPlugin().Has(object)))
			{
				editor.GetPlugin().Remove(object);
				*object = *editor.BaseUniverse().ships.Get(object->TrueName());
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				editor.GetPlugin().Remove(object);
				editor.Universe().ships.Erase(object->TrueName());
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		/*if(ImGui::BeginMenu("Tools"))
		{
			if(ImGui::MenuItem("Add as Escort", nullptr, false, object))
			{
				editor.Player().ships.push_back(make_shared<Ship>(*object));
				editor.Player().ships.back()->SetName(object->TrueName());
				editor.Player().ships.back()->SetSystem(editor.Player().GetSystem());
				editor.Player().ships.back()->SetPlanet(editor.Player().GetPlanet());
				editor.Player().ships.back()->SetIsSpecial();
				editor.Player().ships.back()->SetIsYours();
				editor.Player().ships.back()->SetGovernment(GameData::PlayerGovernment());

				if(auto *panel = dynamic_cast<MainPanel *>(editor.GetUI().Top().get()))
				{
					Fleet::Place(*editor.Player().GetSystem(), *editor.Player().ships.back());
					panel->GetEngine().Place(editor.Player().ships.back());
					editor.Player().ships.back()->SetParent(editor.Player().FlagshipPtr());
				}
			}
			ImGui::EndMenu();
		}*/
		ImGui::EndMenuBar();
	}

	if(showNewShip)
		ImGui::OpenPopup("New Ship");
	if(showRenameShip)
		ImGui::OpenPopup("Rename Ship");
	if(showCloneModel)
		ImGui::OpenPopup("Clone Model");
	if(showCloneVariant)
		ImGui::OpenPopup("Clone Variant");
	ImGui::BeginSimpleNewModal("New Ship", [this](const string &name)
			{
				if(editor.Universe().ships.Find(name))
					return;

				auto *newShip = editor.Universe().ships.Get(name);
				newShip->modelName = name;
				newShip->isDefined = true;
				object = newShip;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Ship", [this](const string &name)
			{
				if(editor.Universe().ships.Find(name))
					return;

				editor.Universe().ships.Rename(object->TrueName(), name);
				if(!object->variantName.empty())
					object->variantName = name;
				else
					object->modelName = name;
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Model", [this](const string &name)
			{
				if(editor.Universe().ships.Find(name))
					return;

				auto *clone = editor.Universe().ships.Get(name);
				*clone = *object;
				object = clone;
				object->modelName = name;
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Variant", [this](const string &name)
			{
				auto *clone = editor.Universe().ships.Get(name);
				*clone = *object;
				object = clone;
				object->variantName = name;
				object->base = editor.Universe().ships.Get(object->modelName);
				SetDirty();
			});

	if(ImGui::InputCombo("ship", &searchBox, &object, editor.Universe().ships))
	{
		searchBox.clear();
		if(auto *panel = dynamic_cast<OutfitterEditorPanel *>(editor.GetUI().Top().get()))
			panel->SetShip(object);
	}

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::PushID(object);
	if(object)
		RenderShip();
	ImGui::PopID();
	ImGui::End();
}



void ShipEditor::RenderShip()
{
	static string buffer;
	static double value;

	if(object->variantName.empty())
		ImGui::Text("model: %s", object->modelName.c_str());
	else
	{
		ImGui::Text("model: %s", object->base->ModelName().c_str());
		ImGui::Text("variant: %s", object->variantName.c_str());
	}
	if(ImGui::InputText("plural", &object->pluralModelName))
		SetDirty();
	if(ImGui::InputText("noun", &object->noun))
		SetDirty();
	RenderElement(object, "sprite");
	static string thumbnail;
	if(object->thumbnail)
		thumbnail = object->thumbnail->Name();
	static Sprite *thumbnailSprite = nullptr;
	if(ImGui::InputCombo("thumbnail", &thumbnail, &thumbnailSprite, editor.Sprites()))
	{
		object->thumbnail = thumbnailSprite;
		SetDirty();
	}
	if(ImGui::Checkbox("never disabled", &object->neverDisabled))
		SetDirty();
	bool uncapturable = !object->isCapturable;
	if(ImGui::Checkbox("uncapturable", &uncapturable))
	{
		object->isCapturable = !uncapturable;
		SetDirty();
	}
	if(ImGui::InputSwizzle("swizzle", &object->customSwizzle, true))
		SetDirty();

	if(ImGui::TreeNode("licenses"))
	{
		int index = 0;
		auto toRemove = object->baseAttributes.licenses.end();
		for(auto it = object->baseAttributes.licenses.begin(); it != object->baseAttributes.licenses.end(); ++it)
		{
			ImGui::PushID(index++);
			if(ImGui::InputText("##license", &*it, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if(it->empty())
					toRemove = it;
				SetDirty();
			}
			ImGui::PopID();
		}
		if(toRemove != object->baseAttributes.licenses.end())
		{
			object->attributes.licenses.erase(object->attributes.licenses.begin() + (toRemove - object->baseAttributes.licenses.begin()));
			object->baseAttributes.licenses.erase(toRemove);
		}
		ImGui::Spacing();

		static string addLicense;
		if(ImGui::InputText("##license", &addLicense, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->baseAttributes.licenses.push_back(addLicense);
			object->attributes.licenses.push_back(std::move(addLicense));
			SetDirty();
		}
		ImGui::TreePop();

	}

	if(ImGui::TreeNode("attributes"))
	{
		if(ImGui::BeginCombo("category", object->baseAttributes.Category().c_str()))
		{
			for(const auto &category : editor.Universe().categories[CategoryType::SHIP])
			{
				const bool selected = object->baseAttributes.Category() == category;
				if(ImGui::Selectable(category.c_str(), selected))
				{
					object->baseAttributes.category = category;
					object->attributes.category = category;
					SetDirty();
				}

				if(selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if(ImGui::InputInt64Ex("cost", &object->baseAttributes.cost))
			SetDirty();

		double oldMass = object->baseAttributes.mass;
		if(ImGui::InputDoubleEx("mass", &object->baseAttributes.mass))
		{
			object->attributes.mass -= oldMass;
			object->attributes.mass += object->baseAttributes.mass;
			SetDirty();
		}

		bool openSprites = ImGui::TreeNode("sprites");
		if(ImGui::BeginPopupContextItem())
		{
			if(ImGui::Selectable("Add flare sprite"))
			{
				object->baseAttributes.flareSprites.emplace_back(Body(), 1);
				SetDirty();
			}
			if(ImGui::Selectable("Add reverse flare sprite"))
			{
				object->baseAttributes.reverseFlareSprites.emplace_back(Body(), 1);
				SetDirty();
			}
			if(ImGui::Selectable("Add steering flare sprite"))
			{
				object->baseAttributes.steeringFlareSprites.emplace_back(Body(), 1);
				SetDirty();
			}
			ImGui::EndPopup();
		}
		if(openSprites)
		{
			RenderSprites("flare sprite", object->baseAttributes.flareSprites);
			RenderSprites("reverse flare sprite", object->baseAttributes.reverseFlareSprites);
			RenderSprites("steering flare sprite", object->baseAttributes.steeringFlareSprites);
			ImGui::TreePop();
		}
		if(ImGui::TreeNode("sounds"))
		{
			RenderSound("flare sound", object->baseAttributes.flareSounds);
			RenderSound("reverse flare sound", object->baseAttributes.reverseFlareSounds);
			RenderSound("steering flare sound", object->baseAttributes.steeringFlareSounds);
			RenderSound("hyperdrive sound", object->baseAttributes.hyperSounds);
			RenderSound("hyperdrive in sound", object->baseAttributes.hyperInSounds);
			RenderSound("hyperdrive out sound", object->baseAttributes.hyperOutSounds);
			RenderSound("jump sound", object->baseAttributes.jumpSounds);
			RenderSound("jump in sound", object->baseAttributes.jumpInSounds);
			RenderSound("jump out sound", object->baseAttributes.jumpOutSounds);
			ImGui::TreePop();
		}
		if(ImGui::TreeNode("effects"))
		{
			RenderEffect("afterburner effect", object->baseAttributes.afterburnerEffects);
			RenderEffect("jump effect", object->baseAttributes.jumpEffects);
			ImGui::TreePop();
		}
		for(auto &it : object->baseAttributes.attributes)
			if(it.second || ImGui::IsInputFocused(it.first))
			{
				value = it.second;
				if(ImGui::InputDoubleEx(it.first, &value))
				{
					object->attributes.attributes[it.first] += value - it.second;
					it.second = value;
					SetDirty();
				}
			}

		ImGui::Spacing();
		static string addAttribute;
		if(ImGui::InputText("add attribute", &addAttribute, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->baseAttributes.Set(addAttribute.c_str(), 1);
			object->attributes.Set(addAttribute.c_str(), 1);
			addAttribute.clear();
			SetDirty();
		}

		ImGui::TreePop();
	}

	if(ImGui::TreeNode("explosion"))
	{
		if(ImGui::InputDoubleEx("blast radius", &object->baseAttributes.blastRadius))
			SetDirty();
		if(ImGui::InputDoubleEx("shield damage", &object->baseAttributes.damage[Weapon::SHIELD_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("hull damage", &object->baseAttributes.damage[Weapon::HULL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("hit force", &object->baseAttributes.damage[Weapon::HIT_FORCE]))
			SetDirty();
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("outfits"))
	{
		auto foundIt = object->outfits.end();
		double val = 0.;
		for(auto it = object->outfits.begin(); it != object->outfits.end(); ++it)
		{
			auto &outfit = *it;

			int amount = outfit.second;
			ImGui::InputInt(outfit.first->Name().c_str(), &amount);
			if(amount < 0)
				amount = 0;
			if(amount != outfit.second)
			{
				foundIt = it;
				val = amount - outfit.second;
			}
		}

		if(foundIt != object->outfits.end())
		{
			object->AddOutfit(foundIt->first, val);
			SetDirty();
		}

		ImGui::Spacing();
		static string addOutfit;
		static Outfit *outfit = nullptr;
		if(ImGui::InputCombo("add outfit", &addOutfit, &outfit, editor.Universe().outfits))
		{
			object->AddOutfit(outfit, 1);
			addOutfit.clear();
			outfit = nullptr;
			SetDirty();
		}
		ImGui::TreePop();
	}

	int index = 0;
	bool engineOpen = ImGui::TreeNode("engines");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Engine"))
		{
			object->enginePoints.emplace_back(0., 0., 0.);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(engineOpen)
	{
		auto found = object->enginePoints.end();
		for(auto it = object->enginePoints.begin(); it != object->enginePoints.end(); ++it)
		{
			ImGui::PushID(index++);
			bool open = ImGui::TreeNode("engine", "engine: %g %g", 2. * it->X(), 2. * it->Y());
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
				double pos[2] = {2. * it->X(), 2. * it->Y()};
				if(ImGui::InputDouble2Ex("position##input", pos))
				{
					it->Set(.5 * pos[0], .5 * pos[1]);
					SetDirty();
				}
				if(ImGui::InputDoubleEx("zoom", &it->zoom))
					SetDirty();
				value = it->facing.Degrees();
				if(ImGui::InputDoubleEx("angle", &value))
					SetDirty();
				it->facing = Angle(value);
				int ivalue = it->side;
				if(ImGui::Combo("side", &ivalue, "under\0over\0\0"))
					SetDirty();
				it->side = !!ivalue;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->enginePoints.end())
			object->enginePoints.erase(found);
		ImGui::TreePop();
	}

	index = 0;
	bool reverseEngineOpen = ImGui::TreeNode("reverse engines");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Reverse Engine"))
		{
			object->reverseEnginePoints.emplace_back(0., 0., 0.);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(reverseEngineOpen)
	{
		auto found = object->reverseEnginePoints.end();
		for(auto it = object->reverseEnginePoints.begin(); it != object->reverseEnginePoints.end(); ++it)
		{
			ImGui::PushID(index++);
			bool open = ImGui::TreeNode("reverse engine", "reverse engine: %g %g", 2. * it->X(), 2. * it->Y());
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
				double pos[2] = {2. * it->X(), 2. * it->Y()};
				if(ImGui::InputDouble2Ex("position##input", pos))
				{
					it->Set(.5 * pos[0], .5 * pos[1]);
					SetDirty();
				}
				if(ImGui::InputDoubleEx("zoom", &it->zoom))
					SetDirty();
				value = it->facing.Degrees() - 180.;
				if(ImGui::InputDoubleEx("angle", &value))
					SetDirty();
				it->facing = Angle(value) + 180.;
				int ivalue = it->side;
				if(ImGui::Combo("side", &ivalue, "under\0over\0\0"))
					SetDirty();
				it->side = !!ivalue;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->reverseEnginePoints.end())
			object->reverseEnginePoints.erase(found);
		ImGui::TreePop();
	}

	index = 0;
	bool steeringEngineOpen = ImGui::TreeNode("steering engines");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Steering Engine"))
		{
			object->steeringEnginePoints.emplace_back(0., 0., 0.);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(steeringEngineOpen)
	{
		auto found = object->steeringEnginePoints.end();
		for(auto it = object->steeringEnginePoints.begin(); it != object->steeringEnginePoints.end(); ++it)
		{
			ImGui::PushID(index++);
			bool open = ImGui::TreeNode("steering engine", "steering engine: %g %g", 2. * it->X(), 2. * it->Y());
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
				double pos[2] = {2. * it->X(), 2. * it->Y()};
				if(ImGui::InputDouble2Ex("position##input", pos))
				{
					it->Set(.5 * pos[0], .5 * pos[1]);
					SetDirty();
				}
				if(ImGui::InputDoubleEx("zoom", &it->zoom))
					SetDirty();
				value = it->facing.Degrees() - 180.;
				if(ImGui::InputDoubleEx("angle", &value))
					SetDirty();
				it->facing = Angle(value) + 180.;
				int ivalue = it->side;
				if(ImGui::Combo("side", &ivalue, "under\0over\0\0"))
					SetDirty();
				it->side = !!ivalue;

				ivalue = it->steering;
				if(ImGui::Combo("facing", &ivalue, "none\0left\0right\0\0"))
					SetDirty();
				it->steering = ivalue;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->steeringEnginePoints.end())
			object->steeringEnginePoints.erase(found);
		ImGui::TreePop();
	}

	index = 0;
	bool hardpointsOpen = ImGui::TreeNode("hardpoints");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Turret"))
		{
			object->armament.hardpoints.emplace_back(Point(), Angle(), true, false, false);
			SetDirty();
		}
		if(ImGui::Selectable("Add Gun"))
		{
			object->armament.hardpoints.emplace_back(Point(), Angle(), false, true, false);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(hardpointsOpen)
	{
		auto found = object->armament.hardpoints.end();
		for(auto it = object->armament.hardpoints.begin(); it != object->armament.hardpoints.end(); ++it)
		{
			ImGui::PushID(index++);
			const char *type = (it->IsTurret() ? "turret" : "gun");
			bool open = it->GetOutfit()
				? ImGui::TreeNode("hardpoint", "%s: %g %g %s", type, 2. * it->GetPoint().X(), 2. * it->GetPoint().Y(), it->GetOutfit()->Name().c_str())
				: ImGui::TreeNode("hardpoint", "%s: %g %g", type, 2. * it->GetPoint().X(), 2. * it->GetPoint().Y());
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
				{
					found = it;
					SetDirty();
				}
				ImGui::EndPopup();
			}

			double hardpointAngle = it->GetBaseAngle().Degrees();
			if(open)
			{
				int ivalue = it->isTurret;
				if(ImGui::Combo("category", &ivalue, "gun\0turret\0\0"))
					SetDirty();
				it->isTurret = !!ivalue;
				double pos[2] = {2. * it->point.X(), 2. * it->point.Y()};
				if(ImGui::InputDouble2Ex("position##input", pos))
				{
					it->point.Set(.5 * pos[0], .5 * pos[1]);
					SetDirty();
				}
				static Outfit *outfit;
				string outfitName = it->GetOutfit() ? it->GetOutfit()->Name() : "";
				if(ImGui::InputCombo("outfit", &outfitName, &outfit, editor.Universe().outfits))
				{
					it->outfit = outfit;
					outfit = nullptr;
					SetDirty();
				}
				if(!it->IsTurret() && ImGui::InputDoubleEx("angle", &hardpointAngle))
					SetDirty();
				if(!it->IsTurret() && ImGui::Checkbox("parallel", &it->isParallel))
					SetDirty();
				ivalue = it->isUnder;
				if(ImGui::Combo("side", &ivalue, "over\0under\0\0"))
					SetDirty();
				it->isUnder = !!ivalue;
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->armament.hardpoints.end())
			object->armament.hardpoints.erase(found);
		ImGui::TreePop();
	}
	index = 0;
	bool baysOpen = ImGui::TreeNode("bays");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Bay"))
		{
			object->bays.emplace_back(0., 0., editor.Universe().categories[CategoryType::BAY][0]);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(baysOpen)
	{
		auto found = object->bays.end();
		for(auto it = object->bays.begin(); it != object->bays.end(); ++it)
		{
			ImGui::PushID(index++);
			bool open = ImGui::TreeNode("bay: %s %g %g", it->category.c_str(), 2. * it->point.X(), 2. * it->point.Y());
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
				if(ImGui::BeginCombo("category", it->category.c_str()))
				{
					for(const auto &category : editor.Universe().categories[CategoryType::BAY])
					{
						const bool selected = it->category == category;
						if(ImGui::Selectable(category.c_str(), selected))
						{
							it->category = category;
							SetDirty();
						}

						if(selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				double pos[2] = {2. * it->point.X(), 2. * it->point.Y()};
				if(ImGui::InputDouble2Ex("pos", pos))
				{
					it->point.Set(.5 * pos[0], .5 * pos[1]);
					SetDirty();
				}
				value = it->facing.Degrees();
				if(ImGui::InputDoubleEx("angle", &value))
					SetDirty();
				it->facing = Angle(value);

				int ivalue = it->side;
				if(ImGui::Combo("bay side", &ivalue, "inside\0over\0under\0\0"))
					SetDirty();
				it->side = ivalue;

				static std::string effectName;
				effectName.clear();
				bool effectOpen = ImGui::TreeNode("launch effects");
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Add Effect"))
					{
						it->launchEffects.emplace_back();
						SetDirty();
					}
					ImGui::EndPopup();
				}
				if(effectOpen)
				{
					const Effect *toAdd = nullptr;
					auto toRemove = it->launchEffects.end();
					int index = 0;
					for(auto jt = it->launchEffects.begin(); jt != it->launchEffects.end(); ++jt)
					{
						ImGui::PushID(index++);

						effectName = *jt ? (*jt)->Name() : "";
						bool effectItselfOpen = ImGui::TreeNode("effect##node", "effect: %s", effectName.c_str());
						if(ImGui::BeginPopupContextItem())
						{
							if(ImGui::Selectable("Remove"))
							{
								toRemove = jt;
								SetDirty();
							}
							ImGui::EndPopup();
						}
						if(effectItselfOpen)
						{
							static Effect *effect;
							if(ImGui::InputCombo("effect", &effectName, &effect, editor.Universe().effects))
								if(!effectName.empty())
								{
									toAdd = effect;
									toRemove = jt;
									SetDirty();
								}
							ImGui::TreePop();
						}
						ImGui::PopID();
					}

					if(toAdd)
						it->launchEffects.push_back(toAdd);
					if(toRemove != it->launchEffects.end())
						it->launchEffects.erase(toRemove);
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->bays.end())
			object->bays.erase(found);
		ImGui::TreePop();
	}

	bool leaksOpen = ImGui::TreeNode("leaks");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Leak"))
		{
			object->leaks.emplace_back();
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(leaksOpen)
	{
		index = 0;
		auto found = object->leaks.end();
		for(auto it = object->leaks.begin(); it != object->leaks.end(); ++it)
		{
			ImGui::PushID(index++);
			static string effectName;
			effectName = it->effect ? it->effect->Name() : "";
			bool open = ImGui::TreeNode("leak", "leak: %s %d %d", effectName.c_str(), it->openPeriod, it->closePeriod);
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
				static Effect *effect;
				if(ImGui::InputCombo("leak##input", &effectName, &effect, editor.Universe().effects))
					if(!effectName.empty())
					{
						it->effect = effect;
						SetDirty();
					}
				if(ImGui::InputInt("open period", &it->openPeriod))
					SetDirty();
				if(ImGui::InputInt("close period", &it->closePeriod))
					SetDirty();
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		if(found != object->leaks.end())
			object->leaks.erase(found);
		ImGui::TreePop();
	}

	RenderEffect("explode", object->explosionEffects);
	RenderEffect("final explode", object->finalExplosions);

	if(ImGui::InputTextMultiline("description", &object->description))
		SetDirty();
}



void ShipEditor::WriteToFile(DataWriter &writer, const Ship *ship) const
{
	const auto *diff = editor.BaseUniverse().ships.Has(ship->TrueName())
		? editor.BaseUniverse().ships.Get(ship->TrueName())
		: nullptr;

	// We might have a variant here so we need to select the correct base ship.
	if(!diff && !ship->variantName.empty())
	{
		if(editor.BaseUniverse().ships.Has(ship->ModelName()))
			diff = editor.BaseUniverse().ships.Get(ship->ModelName());
		else if(editor.Universe().ships.Has(ship->ModelName()))
			diff = editor.Universe().ships.Get(ship->ModelName());
	}

	if(ship->variantName.empty())
		writer.Write("ship", ship->modelName);
	else
		writer.Write("ship", ship->base->ModelName(), ship->variantName);
	writer.BeginChild();
	if(!diff || ship->pluralModelName != diff->pluralModelName)
		if(ship->pluralModelName != ship->modelName + 's')
			writer.Write("plural", ship->pluralModelName);
	if(!diff || ship->noun != diff->noun)
		if(!ship->noun.empty())
			writer.Write("noun", ship->noun);
	if(!diff || ship->sprite != diff->sprite)
		ship->SaveSprite(writer);
	if(!diff || ship->thumbnail != diff->thumbnail)
		if(ship->thumbnail)
			writer.Write("thumbnail", ship->thumbnail->Name());

	if(!diff || ship->neverDisabled != diff->neverDisabled)
		if(ship->neverDisabled)
			writer.Write("never disabled");
	if(!diff || ship->isCapturable!= diff->isCapturable)
		if(!ship->isCapturable)
			writer.Write("uncapturable");
	if(!diff || ship->customSwizzle != diff->customSwizzle)
		if(ship->customSwizzle >= 0)
			writer.Write("swizzle", ship->customSwizzle);

	bool hasWrittenAttributes = false;
	auto writeAttributes = [&hasWrittenAttributes, &writer, &diff] ()
	{
		if(!hasWrittenAttributes)
		{
			if(diff)
				writer.Write("add", "attributes");
			else
				writer.Write("attributes");
			writer.BeginChild();
			hasWrittenAttributes = true;
		}
	};

	if(!diff || ship->baseAttributes.Category() != diff->baseAttributes.Category())
		if(!ship->baseAttributes.Category().empty() || diff)
		{
			writeAttributes();
			writer.Write("category", ship->baseAttributes.Category());
		}
	if(!diff || ship->attributes.licenses != diff->attributes.licenses)
		if(!ship->attributes.Licenses().empty())
		{
			writeAttributes();
			writer.WriteToken("licenses");
			for(auto &&license : ship->baseAttributes.Licenses())
				if(!diff || !diff->attributes.Get(license))
					writer.WriteToken(license);
			writer.Write();
		}
	if(!diff || ship->baseAttributes.Cost() != diff->baseAttributes.Cost())
		if(ship->baseAttributes.Cost() || diff)
		{
			writeAttributes();
			writer.Write("cost", ship->baseAttributes.Cost() - (diff ? diff->baseAttributes.Cost() : 0.));
		}
	if(!diff || ship->baseAttributes.Mass() != diff->baseAttributes.Mass())
		if(ship->baseAttributes.Mass() || diff)
		{
			writeAttributes();
			writer.WriteQuoted("mass", ship->baseAttributes.Mass() - (diff ? diff->baseAttributes.Mass() : 0.));
		}

	if(!diff || ship->baseAttributes.FlareSprites() != diff->baseAttributes.FlareSprites())
		for(const auto &it : ship->baseAttributes.FlareSprites())
			if(!diff || !Count(diff->baseAttributes.FlareSprites(), it))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					it.first.SaveSprite(writer, "flare sprite");
				}
	if(!diff || ship->baseAttributes.FlareSounds() != diff->baseAttributes.FlareSounds())
		for(const auto &it : ship->baseAttributes.FlareSounds())
			if(!diff || !diff->baseAttributes.FlareSounds().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("flare sound", it.first->Name());
				}
	if(!diff || ship->baseAttributes.ReverseFlareSprites() != diff->baseAttributes.ReverseFlareSprites())
		for(const auto &it : ship->baseAttributes.ReverseFlareSprites())
			if(!diff || !Count(diff->baseAttributes.ReverseFlareSprites(), it))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					it.first.SaveSprite(writer, "reverse flare sprite");
				}
	if(!diff || ship->baseAttributes.ReverseFlareSounds() != diff->baseAttributes.ReverseFlareSounds())
		for(const auto &it : ship->baseAttributes.ReverseFlareSounds())
			if(!diff || !diff->baseAttributes.ReverseFlareSounds().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("reverse flare sound", it.first->Name());
				}
	if(!diff || ship->baseAttributes.SteeringFlareSprites() != diff->baseAttributes.SteeringFlareSprites())
		for(const auto &it : ship->baseAttributes.SteeringFlareSprites())
			if(!diff || !Count(diff->baseAttributes.SteeringFlareSprites(), it))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					it.first.SaveSprite(writer, "steering flare sprite");
				}
	if(!diff || ship->baseAttributes.SteeringFlareSounds() != diff->baseAttributes.SteeringFlareSounds())
		for(const auto &it : ship->baseAttributes.SteeringFlareSounds())
			if(!diff || !diff->baseAttributes.SteeringFlareSounds().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("steering flare sound", it.first->Name());
				}
	if(!diff || ship->baseAttributes.AfterburnerEffects() != diff->baseAttributes.AfterburnerEffects())
		for(const auto &it : ship->baseAttributes.AfterburnerEffects())
			if(!diff || !diff->baseAttributes.AfterburnerEffects().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("afterburner effect", it.first->Name());
				}
	if(!diff || ship->baseAttributes.JumpEffects() != diff->baseAttributes.JumpEffects())
		for(const auto &it : ship->baseAttributes.JumpEffects())
			if(!diff || !diff->baseAttributes.JumpEffects().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("jump effect", it.first->Name());
				}
	if(!diff || ship->baseAttributes.JumpSounds() != diff->baseAttributes.JumpSounds())
		for(const auto &it : ship->baseAttributes.JumpSounds())
			if(!diff || !diff->baseAttributes.JumpSounds().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("jump sound", it.first->Name());
				}
	if(!diff || ship->baseAttributes.JumpInSounds() != diff->baseAttributes.JumpInSounds())
		for(const auto &it : ship->baseAttributes.JumpInSounds())
			if(!diff || !diff->baseAttributes.JumpInSounds().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("jump in sound", it.first->Name());
				}
	if(!diff || ship->baseAttributes.JumpOutSounds() != diff->baseAttributes.JumpOutSounds())
		for(const auto &it : ship->baseAttributes.JumpOutSounds())
			if(!diff || !diff->baseAttributes.JumpOutSounds().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("jump out sound", it.first->Name());
				}
	if(!diff || ship->baseAttributes.HyperSounds() != diff->baseAttributes.HyperSounds())
		for(const auto &it : ship->baseAttributes.HyperSounds())
			if(!diff || !diff->baseAttributes.HyperSounds().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("hyperdrive sound", it.first->Name());
				}
	if(!diff || ship->baseAttributes.HyperInSounds() != diff->baseAttributes.HyperInSounds())
		for(const auto &it : ship->baseAttributes.HyperInSounds())
			if(!diff || !diff->baseAttributes.HyperInSounds().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("hyperdrive in sound", it.first->Name());
				}
	if(!diff || ship->baseAttributes.HyperOutSounds() != diff->baseAttributes.HyperOutSounds())
		for(const auto &it : ship->baseAttributes.HyperOutSounds())
			if(!diff || !diff->baseAttributes.HyperOutSounds().count(it.first))
				for(int i = 0; i < it.second; ++i)
				{
					writeAttributes();
					writer.Write("hyperdrive out sound", it.first->Name());
				}

	auto shipAttributes = ship->baseAttributes.Attributes();
	auto diffAttributes = diff ? diff->baseAttributes.Attributes() : shipAttributes;
	shipAttributes.Remove("gun ports");
	shipAttributes.Remove("turret mounts");
	diffAttributes.Remove("gun ports");
	diffAttributes.Remove("turret mounts");

	if(!diff || shipAttributes.AsBase() != diffAttributes.AsBase())
	{
		// Write the known attributes first.
		for(string_view attribute : OutfitEditor::AttributesOrder())
			if(auto val = shipAttributes.Get(attribute.data()))
				if(!diff || !Count(diffAttributes.AsBase(), make_pair(attribute.data(), val)))
					writer.WriteQuoted(attribute.data(), val);

		// And unsorted attributes get put in the end.
		for(auto it = shipAttributes.begin(); it != shipAttributes.end(); ++it)
			if(auto ait = find(OutfitEditor::AttributesOrder().begin(), OutfitEditor::AttributesOrder().end(), it->first);
					ait == OutfitEditor::AttributesOrder().end())
				if(!diff || !Count(diffAttributes.AsBase(), *it))
				{
					writeAttributes();
					writer.WriteQuoted(it->first, it->second - (diff ? diffAttributes.Get(it->first) : 0.));
				}
	}

	bool hasWrittenWeapon = false;
	auto writeWeapon = [&hasWrittenWeapon, &writer]()
	{
		if(!hasWrittenWeapon)
		{
			writer.Write("weapon");
			writer.BeginChild();
			hasWrittenWeapon = true;
		}
	};
	if(!diff || ship->explosionWeapon->BlastRadius() != diff->explosionWeapon->BlastRadius())
		if(ship->explosionWeapon->BlastRadius() || diff)
		{
			writeAttributes();
			writeWeapon();
			writer.Write("blast radius", ship->explosionWeapon->BlastRadius());
		}
	if(!diff || ship->explosionWeapon->ShieldDamage() != diff->explosionWeapon->ShieldDamage())
		if(ship->explosionWeapon->ShieldDamage() || diff)
		{
			writeAttributes();
			writeWeapon();
			writer.Write("shield damage", ship->explosionWeapon->ShieldDamage());
		}
	if(!diff || ship->explosionWeapon->HullDamage() != diff->explosionWeapon->HullDamage())
		if(ship->explosionWeapon->HullDamage() || diff)
		{
			writeAttributes();
			writeWeapon();
			writer.Write("hull damage", ship->explosionWeapon->HullDamage());
		}
	if(!diff || ship->explosionWeapon->HitForce() != diff->explosionWeapon->HitForce())
		if(ship->explosionWeapon->HitForce() || diff)
		{
			writeAttributes();
			writeWeapon();
			writer.Write("hit force", ship->explosionWeapon->HitForce());
		}

	if(hasWrittenWeapon)
		writer.EndChild();
	if(hasWrittenAttributes)
		writer.EndChild();

	if(!diff || ship->outfits != diff->outfits)
		if(!ship->outfits.empty() || diff)
		{
			writer.Write("outfits");
			writer.BeginChild();
			{
				using OutfitElement = pair<const Outfit *const, int>;
				WriteSorted(ship->outfits,
					[](const OutfitElement *lhs, const OutfitElement *rhs)
						{ return lhs->first->Name() < rhs->first->Name(); },
					[&writer](const OutfitElement &it){
						if(it.second == 1)
							writer.Write(it.first->Name());
						else
							writer.Write(it.first->Name(), it.second);
					});
			}
			writer.EndChild();
		}

	if(!diff || ship->enginePoints != diff->enginePoints
			|| ship->reverseEnginePoints != diff->reverseEnginePoints
			|| ship->steeringEnginePoints != diff->steeringEnginePoints)
	{
		for(const auto &point : ship->enginePoints)
		{
			writer.Write("engine", 2. * point.X(), 2. * point.Y());
			writer.BeginChild();
			if(point.zoom != 1.)
				writer.Write("zoom", point.zoom);
			if(point.facing.Degrees())
				writer.Write("angle", point.facing.Degrees());
			if(point.side)
				writer.Write("over");
			writer.EndChild();
		}
		for(const auto &point : ship->reverseEnginePoints)
		{
			writer.Write("reverse engine", 2. * point.X(), 2. * point.Y());
			writer.BeginChild();
			if(point.zoom != 1.)
				writer.Write("zoom", point.zoom);
			if(point.facing.Degrees())
				writer.Write("angle", point.facing.Degrees() - 180.);
			if(point.side)
				writer.Write("over");
			writer.EndChild();
		}
		for(const auto &point : ship->steeringEnginePoints)
		{
			writer.Write("steering engine", 2. * point.X(), 2. * point.Y());
			writer.BeginChild();
			if(point.zoom != 1.)
				writer.Write("zoom", point.zoom);
			if(point.facing.Degrees())
				writer.Write("angle", point.facing.Degrees());
			if(point.side)
				writer.Write("over");
			if(point.steering)
				writer.Write(point.steering == 1 ? "left" : "right");
			writer.EndChild();
		}
	}
	if(!diff || ship->armament != diff->armament)
	{
		auto shipArmament = ship->armament;
		shipArmament.UninstallAll();
		for(const auto &outfit : ship->outfits)
			if(outfit.first->IsWeapon())
				shipArmament.Add(outfit.first, outfit.second);

		auto emptyShip = ship->armament;
		emptyShip.UninstallAll();
		auto emptyDiff = diff ? diff->armament : emptyShip;
		emptyDiff.UninstallAll();
		if(!diff || shipArmament != ship->armament || emptyShip != emptyDiff)
			for(const Hardpoint &hardpoint : ship->armament.Get())
			{
				const char *type = (hardpoint.IsTurret() ? "turret" : "gun");
				if(hardpoint.GetOutfit())
					writer.Write(type, 2. * hardpoint.GetPoint().X(), 2. * hardpoint.GetPoint().Y(),
						hardpoint.GetOutfit()->Name());
				else
					writer.Write(type, 2. * hardpoint.GetPoint().X(), 2. * hardpoint.GetPoint().Y());
				double hardpointAngle = hardpoint.GetBaseAngle().Degrees();
				writer.BeginChild();
				{
					if(hardpointAngle)
						writer.Write("angle", hardpointAngle);
					if(hardpoint.IsParallel())
						writer.Write("parallel");
					if(hardpoint.IsUnder() && hardpoint.IsTurret())
						writer.Write("under");
					else if(!hardpoint.IsUnder() && !hardpoint.IsTurret())
						writer.Write("over");
				}
				writer.EndChild();
			}
	}
	if(!diff || ship->bays != diff->bays)
		for(const auto &bay : ship->bays)
		{
			double x = 2. * bay.point.X();
			double y = 2. * bay.point.Y();

			writer.Write("bay", bay.category, x, y);
			if(!bay.launchEffects.empty() || bay.facing.Degrees() || bay.side)
			{
				writer.BeginChild();
				{
					if(bay.facing.Degrees() == -90.)
						writer.Write("left");
					else if(bay.facing.Degrees() == 90.)
						writer.Write("right");
					else if(bay.facing.Degrees() == 180.)
						writer.Write("back");
					else if(bay.facing.Degrees())
						writer.Write("angle", bay.facing.Degrees());

					if(bay.side)
						writer.Write(!bay.side ? "inside" : bay.side == 1 ? "over" : "under");
					for(const Effect *effect : bay.launchEffects)
						if(effect->Name() != "basic launch")
							writer.Write("launch effect", effect->Name());
				}
				writer.EndChild();
			}
		}
	if(!diff || ship->leaks != diff->leaks)
		for(const auto &leak : ship->leaks)
			writer.Write("leak", leak.effect->Name(), leak.openPeriod, leak.closePeriod);

	using EffectElement = pair<const Effect *const, int>;
	auto effectSort = [](const EffectElement *lhs, const EffectElement *rhs)
		{ return lhs->first->Name() < rhs->first->Name(); };
	if(!diff || ship->explosionEffects != diff->explosionEffects)
		WriteSorted(ship->explosionEffects, effectSort, [&writer](const EffectElement &it)
		{
			if(it.second == 1)
				writer.Write("explode", it.first->Name());
			else if(it.second)
				writer.Write("explode", it.first->Name(), it.second);
		});
	if(!diff || ship->finalExplosions != diff->finalExplosions)
		WriteSorted(ship->finalExplosions, effectSort, [&writer](const EffectElement &it)
		{
			if(it.second == 1)
				writer.Write("final explode", it.first->Name());
			else if(it.second)
				writer.Write("final explode", it.first->Name(), it.second);
		});
	if(!diff || ship->description != diff->description)
		if(!ship->description.empty())
		{
			size_t newline = ship->description.find('\n');
			size_t start = 0;
			do {
				string toWrite = ship->description.substr(start, newline - start);
				if(toWrite.empty())
					break;
				writer.Write("description", toWrite);

				start = newline + 1;
				newline = ship->description.find('\n', start);
			} while(newline != string::npos);
		}

	writer.EndChild();
}
