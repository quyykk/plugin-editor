/* EffectEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "EffectEditor.h"

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



EffectEditor::EffectEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Effect>(editor, show)
{
}



void EffectEditor::Render()
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Effect Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	bool showNewEffect = false;
	bool showRenameEffect = false;
	bool showCloneEffect = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Effect"))
		{
			const bool alreadyDefined = object && !editor.BaseUniverse().effects.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewEffect);
			ImGui::MenuItem("Rename", nullptr, &showRenameEffect, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneEffect, object);
			if(ImGui::MenuItem("Reset", nullptr, false, alreadyDefined && editor.GetPlugin().Has(object)))
			{
				editor.GetPlugin().Remove(object);
				*object = *editor.BaseUniverse().effects.Get(object->name);
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				editor.GetPlugin().Remove(object);
				editor.Universe().effects.Erase(object->name);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewEffect)
		ImGui::OpenPopup("New Effect");
	if(showRenameEffect)
		ImGui::OpenPopup("Rename Effect");
	if(showCloneEffect)
		ImGui::OpenPopup("Clone Effect");
	ImGui::BeginSimpleNewModal("New Effect", [this](const string &name)
			{
				if(editor.Universe().effects.Find(name))
					return;

				auto *newEffect = editor.Universe().effects.Get(name);
				newEffect->name = name;
				object = newEffect;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Effect", [this](const string &name)
			{
				if(editor.Universe().effects.Find(name))
					return;

				editor.Universe().effects.Rename(object->name, name);
				object->name = name;
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Effect", [this](const string &name)
			{
				if(editor.Universe().effects.Find(name))
					return;

				auto *clone = editor.Universe().effects.Get(name);
				*clone = *object;
				object = clone;

				object->name = name;
				SetDirty();
			});

	if(ImGui::InputCombo("effect", &searchBox, &object, editor.Universe().effects))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::PushID(object);
	if(object)
		RenderEffect();
	ImGui::PopID();
	ImGui::End();
}



void EffectEditor::RenderEffect()
{
	ImGui::Text("effect: %s", object->name.c_str());
	RenderElement(object, "sprite");

	string soundName = object->sound ? object->sound->Name() : "";
	if(ImGui::InputCombo("sound", &soundName, &object->sound, editor.Sounds()))
		SetDirty();

	if(ImGui::InputInt("lifetime", &object->lifetime))
		SetDirty();
	if(ImGui::InputInt("random lifetime", &object->randomLifetime))
		SetDirty();
	if(ImGui::InputDoubleEx("velocity scale", &object->velocityScale))
		SetDirty();
	if(ImGui::InputDoubleEx("random velocity", &object->randomVelocity))
		SetDirty();
	if(ImGui::InputDoubleEx("random angle", &object->randomAngle))
		SetDirty();
	if(ImGui::InputDoubleEx("random spin", &object->randomSpin))
		SetDirty();
	if(ImGui::InputDoubleEx("random frame rate", &object->randomFrameRate))
		SetDirty();
}



void EffectEditor::WriteToFile(DataWriter &writer, const Effect *effect) const
{
	const auto *diff = editor.BaseUniverse().effects.Has(effect->name)
		? editor.BaseUniverse().effects.Get(effect->name)
		: nullptr;

	writer.Write("effect", effect->name);
	writer.BeginChild();

	if(!diff || effect->sprite != diff->sprite)
		if(effect->HasSprite())
		{
			writer.Write("sprite", effect->GetSprite()->Name());
			writer.BeginChild();
			if(effect->frameRate != 2.f / 60.f)
				writer.Write("frame rate", effect->frameRate * 60.f);
			if(effect->delay)
				writer.Write("delay", effect->delay);
			if(effect->randomize)
				writer.Write("random start frame");
			if(!effect->repeat)
				writer.Write("no repeat");
			if(effect->rewind)
				writer.Write("rewind");
			writer.EndChild();
		}

	if(!diff || effect->sound != diff->sound)
		if(effect->sound)
			writer.Write("sound", effect->sound->Name());
	if(!diff || effect->lifetime != diff->lifetime)
		if(effect->lifetime || diff)
			writer.Write("lifetime", effect->lifetime);
	if(!diff || effect->randomLifetime != diff->randomLifetime)
		if(effect->randomLifetime || diff)
			writer.Write("random lifetime", effect->randomLifetime);
	if(!diff || effect->velocityScale != diff->velocityScale)
		if(effect->velocityScale != 1. || diff)
			writer.Write("velocity scale", effect->velocityScale);
	if(!diff || effect->randomAngle != diff->randomAngle)
		if(effect->randomAngle || diff)
			writer.Write("random angle", effect->randomAngle);
	if(!diff || effect->randomVelocity != diff->randomVelocity)
		if(effect->randomVelocity || diff)
			writer.Write("random velocity", effect->randomVelocity);
	if(!diff || effect->randomSpin != diff->randomSpin)
		if(effect->randomSpin || diff)
			writer.Write("random spin", effect->randomSpin);
	if(!diff || effect->randomFrameRate != diff->randomFrameRate)
		if(effect->randomFrameRate || diff)
			writer.Write("random frame rate", effect->randomFrameRate);
	writer.EndChild();
}
