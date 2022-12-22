// SPDX-License-Identifier: GPL-3.0

#include "TemplateEditor.h"

#include "Body.h"
#include "DataWriter.h"
#include "Editor.h"
#include "Effect.h"
#include "Fleet.h"
#include "GameData.h"
#include "Minable.h"
#include "RandomEvent.h"
#include "Sound.h"
#include "Sprite.h"
#include "System.h"
#include "WeightedList.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_stdlib.h"

#include <list>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>



template <typename T>
void TemplateEditor<T>::Clear()
{
	searchBox.clear();
	object = nullptr;
}



template <typename T>
void TemplateEditor<T>::SetDirty()
{
	editor.GetPlugin().Add(object);
}



template <typename T>
void TemplateEditor<T>::SetDirty(const T *obj)
{
	editor.GetPlugin().Add(obj);
}



template <typename T>
void TemplateEditor<T>::RenderSprites(const std::string &name, std::vector<std::pair<Body, int>> &map)
{
	auto found = map.end();
	int index = 0;
	ImGui::PushID(name.c_str());
	for(auto it = map.begin(); it != map.end(); ++it)
	{
		ImGui::PushID(index++);
		if(RenderElement(&it->first, name))
			found = it;
		ImGui::PopID();
	}
	ImGui::PopID();

	if(found != map.end())
	{
		map.erase(found);
		SetDirty();
	}
}



template <typename T>
bool TemplateEditor<T>::RenderElement(Body *sprite, const std::string &name)
{
	static std::string spriteName;
	spriteName.clear();
	bool open = ImGui::TreeNode(name.c_str(), "%s: %s", name.c_str(), sprite->GetSprite() ? sprite->GetSprite()->Name().c_str() : "");
	bool value = false;
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Remove"))
			value = true;
		ImGui::EndPopup();
	}

	if(open)
	{
		if(sprite->GetSprite())
			spriteName = sprite->GetSprite()->Name();
		if(ImGui::InputCombo("sprite", &spriteName, &sprite->sprite, editor.Sprites()))
			SetDirty();

		if(ImGui::InputFloatEx("scale", &sprite->scale))
			SetDirty();

		double value = sprite->frameRate * 60.;
		if(ImGui::InputDoubleEx("frame rate", &value))
		{
			if(value)
				sprite->frameRate = value / 60.;
			SetDirty();
		}

		if(ImGui::InputInt("delay", &sprite->delay))
			SetDirty();
		if(ImGui::Checkbox("random start frame", &sprite->randomize))
			SetDirty();
		bool bvalue = !sprite->repeat;
		if(ImGui::Checkbox("no repeat", &bvalue))
			SetDirty();
		sprite->repeat = !bvalue;
		if(ImGui::Checkbox("rewind", &sprite->rewind))
			SetDirty();
		ImGui::TreePop();
	}

	return value;
}



template <typename T>
void TemplateEditor<T>::RenderSound(const std::string &name, std::map<const Sound *, int> &map)
{
	static std::string soundName;
	soundName.clear();

	if(ImGui::TreeNode(name.c_str()))
	{
		const Sound *toAdd = nullptr;
		auto toRemove = map.end();
		int index = 0;
		for(auto it = map.begin(); it != map.end(); ++it)
		{
			ImGui::PushID(index++);

			soundName = it->first ? it->first->Name() : "";
			bool open = ImGui::TreeNode("sound", "%s", soundName.c_str());
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
				if(ImGui::InputCombo("sound", &soundName, &toAdd, editor.Sounds()))
				{
					toRemove = it;
					SetDirty();
				}
				if(ImGui::InputInt("count", &it->second))
				{
					if(!it->second)
						toRemove = it;
					SetDirty();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		static std::string newSoundName;
		static const Sound *newSound;
		if(ImGui::InputCombo("new sound", &newSoundName, &newSound, editor.Sounds()))
			if(!newSoundName.empty())
			{
				++map[newSound];
				newSoundName.clear();
				newSound = nullptr;
				SetDirty();
			}

		if(toAdd)
		{
			map[toAdd] += map[toRemove->first];
			map.erase(toRemove);
		}
		else if(toRemove != map.end())
			map.erase(toRemove);
		ImGui::TreePop();
	}
}



template <typename T>
void TemplateEditor<T>::RenderEffect(const std::string &name, std::map<const Effect *, int> &map)
{
	static std::string effectName;
	effectName.clear();

	if(ImGui::TreeNode(name.c_str()))
	{
		Effect *toAdd = nullptr;
		auto toRemove = map.end();
		int index = 0;
		for(auto it = map.begin(); it != map.end(); ++it)
		{
			ImGui::PushID(index++);

			effectName = it->first->Name();
			bool open = ImGui::TreeNode("effect", "%s", effectName.c_str());
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
				if(ImGui::InputCombo("effect", &effectName, &toAdd, GameData::Effects()))
				{
					toRemove = it;
					SetDirty();
				}
				if(ImGui::InputInt("count", &it->second, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue))
				{
					if(!it->second)
						toRemove = it;
					SetDirty();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		static std::string newEffectName;
		static const Effect *newEffect;
		if(ImGui::InputCombo("new effect", &newEffectName, &newEffect, GameData::Effects()))
			if(!newEffectName.empty())
			{
				++map[newEffect];
				newEffectName.clear();
				newEffect = nullptr;
				SetDirty();
			}
		if(toAdd)
		{
			map[toAdd] += map[toRemove->first];
			map.erase(toRemove);
		}
		else if(toRemove != map.end())
			map.erase(toRemove);
		ImGui::TreePop();
	}
}



template class TemplateEditor<Effect>;
template class TemplateEditor<Fleet>;
template class TemplateEditor<Galaxy>;
template class TemplateEditor<Hazard>;
template class TemplateEditor<Government>;
template class TemplateEditor<Outfit>;
template class TemplateEditor<Sale<Outfit>>;
template class TemplateEditor<Planet>;
template class TemplateEditor<Ship>;
template class TemplateEditor<Sale<Ship>>;
template class TemplateEditor<System>;
