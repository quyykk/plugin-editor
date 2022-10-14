// SPDX-License-Identifier: GPL-3.0

#include "GalaxyEditor.h"

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
#include "Galaxy.h"
#include "Government.h"
#include "MainPanel.h"
#include "MapEditorPanel.h"
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



GalaxyEditor::GalaxyEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Galaxy>(editor, show)
{
}



void GalaxyEditor::Render()
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Galaxy Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	bool showNewGalaxy = false;
	bool showRenameGalaxy = false;
	bool showCloneGalaxy = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Galaxy"))
		{
			const bool alreadyDefined = object && !editor.BaseUniverse().galaxies.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewGalaxy);
			ImGui::MenuItem("Rename", nullptr, &showRenameGalaxy, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneGalaxy, object);
			if(ImGui::MenuItem("Reset", nullptr, false, alreadyDefined && editor.GetPlugin().Has(object)))
			{
				editor.GetPlugin().Remove(object);
				*object = *editor.BaseUniverse().galaxies.Get(object->name);
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				editor.GetPlugin().Remove(object);
				editor.Universe().galaxies.Erase(object->name);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Tools"))
		{
			if(ImGui::MenuItem("Go to Galaxy", nullptr, false, object))
				if(auto *panel = dynamic_cast<MapEditorPanel *>(editor.GetUI().Top().get()))
					panel->Select(object);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewGalaxy)
		ImGui::OpenPopup("New Galaxy");
	if(showRenameGalaxy)
		ImGui::OpenPopup("Rename Galaxy");
	if(showCloneGalaxy)
		ImGui::OpenPopup("Clone Galaxy");
	ImGui::BeginSimpleNewModal("New Galaxy", [this](const string &name)
			{
				if(editor.Universe().galaxies.Find(name))
					return;

				auto *newGalaxy = editor.Universe().galaxies.Get(name);
				newGalaxy->name = name;
				object = newGalaxy;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Galaxy", [this](const string &name)
			{
				if(editor.Universe().galaxies.Find(name))
					return;

				editor.Universe().galaxies.Rename(object->name, name);
				object->name = name;
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Galaxy", [this](const string &name)
			{
				if(editor.Universe().galaxies.Find(name))
					return;

				auto *clone = editor.Universe().galaxies.Get(name);
				*clone = *object;
				object = clone;

				object->name = name;
				SetDirty();
			});

	if(ImGui::InputCombo("galaxy", &searchBox, &object, editor.Universe().galaxies))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::PushID(object);
	if(object)
		RenderGalaxy();
	ImGui::PopID();
	ImGui::End();
}



void GalaxyEditor::RenderGalaxy()
{
	ImGui::Text("name: %s", object->name.c_str());

	double pos[2] = {object->position.X(), object->Position().Y()};
	if(ImGui::InputDouble2Ex("pos", pos))
	{
		object->position.Set(pos[0], pos[1]);
		SetDirty();
	}

	string spriteName = object->sprite ? object->sprite->Name() : "";
	if(ImGui::InputCombo("sprite", &spriteName, &object->sprite, editor.Sprites()))
		SetDirty();

	// TODO: Maybe show the galaxy image here. ImGui doesn't support texture arrays (which
	// we use to draw sprites), so we'd have to hack something in. *pain*
}



void GalaxyEditor::WriteToFile(DataWriter &writer, const Galaxy *galaxy) const
{
	const auto *diff = editor.BaseUniverse().galaxies.Has(galaxy->name)
		? editor.BaseUniverse().galaxies.Get(galaxy->name)
		: nullptr;

	writer.Write("galaxy", galaxy->Name());
	writer.BeginChild();

	if(!diff || (diff && (galaxy->position.X() != diff->position.X() || galaxy->position.Y() != diff->position.Y())))
		if(galaxy->position || diff)
			writer.Write("pos", galaxy->position.X(), galaxy->position.Y());
	if(!diff || galaxy->sprite != diff->sprite)
		if(galaxy->sprite)
			writer.Write("sprite", galaxy->sprite->Name());

	writer.EndChild();
}
