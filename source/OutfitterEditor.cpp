// SPDX-License-Identifier: GPL-3.0

#include "OutfitterEditor.h"

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



OutfitterEditor::OutfitterEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Sale<Outfit>>(editor, show)
{
}



void OutfitterEditor::Render()
{
	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Outfitter Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	bool showNewOutfitter = false;
	bool showRenameOutfitter = false;
	bool showCloneOutfitter = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Outfitter"))
		{
			const bool alreadyDefined = object && !editor.BaseUniverse().outfitSales.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewOutfitter);
			ImGui::MenuItem("Rename", nullptr, &showRenameOutfitter, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneOutfitter, object);
			if(ImGui::MenuItem("Reset", nullptr, false, alreadyDefined && editor.GetPlugin().Has(object)))
			{
				editor.GetPlugin().Remove(object);
				*object = *editor.BaseUniverse().outfitSales.Get(object->name);
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				editor.GetPlugin().Remove(object);
				editor.Universe().outfitSales.Erase(object->name);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewOutfitter)
		ImGui::OpenPopup("New Outfitter");
	if(showRenameOutfitter)
		ImGui::OpenPopup("Rename Outfitter");
	if(showCloneOutfitter)
		ImGui::OpenPopup("Clone Outfitter");
	ImGui::BeginSimpleNewModal("New Outfitter", [this](const string &name)
			{
				if(editor.Universe().outfitSales.Find(name))
					return;

				auto *newOutfitter = editor.Universe().outfitSales.Get(name);
				newOutfitter->name = name;
				object = newOutfitter;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Outfitter", [this](const string &name)
			{
				if(editor.Universe().outfitSales.Find(name))
					return;

				editor.Universe().outfitSales.Rename(object->name, name);
				object->name = name;
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Outfitter", [this](const string &name)
			{
				if(editor.Universe().outfitSales.Find(name))
					return;

				auto *clone = editor.Universe().outfitSales.Get(name);
				*clone = *object;
				object = clone;

				object->name = name;
				SetDirty();
			});

	if(ImGui::InputCombo("outfitter", &searchBox, &object, editor.Universe().outfitSales))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	ImGui::PushID(object);
	if(object)
		RenderOutfitter();
	ImGui::PopID();
	ImGui::End();
}



void OutfitterEditor::RenderOutfitter()
{
	ImGui::Text("name: %s", object->name.c_str());
	int index = 0;
	const Outfit *toAdd = nullptr;
	const Outfit *toRemove = nullptr;
	for(auto it = object->begin(); it != object->end(); ++it)
	{
		ImGui::PushID(index++);
		string name = (*it)->Name();
		Outfit *change = nullptr;
		if(ImGui::InputCombo("##outfit", &name, &change, editor.Universe().outfits))
		{
			if(name.empty())
			{
				toRemove = *it;
				SetDirty();
			}
			else
			{
				toAdd = change;
				toRemove = *it;
				SetDirty();
			}
		}
		ImGui::PopID();
	}
	if(toAdd)
		object->insert(toAdd);
	if(toRemove)
		object->erase(toRemove);

	static string outfitName;
	static Outfit *outfit = nullptr;
	ImGui::Spacing();
	if(ImGui::InputCombo("add outfit", &outfitName, &outfit, editor.Universe().outfits))
		if(!outfitName.empty())
		{
			object->insert(outfit);
			SetDirty();
			outfitName.clear();
		}
}



void OutfitterEditor::WriteToFile(DataWriter &writer, const Sale<Outfit> *outfitter) const
{
	const auto *diff = editor.BaseUniverse().outfitSales.Has(outfitter->name)
		? editor.BaseUniverse().outfitSales.Get(outfitter->name)
		: nullptr;

	writer.Write("outfitter", outfitter->name);
	writer.BeginChild();
	if(diff)
		WriteSorted(diff->AsBase(), [](const Outfit *lhs, const Outfit *rhs) { return lhs->Name() < rhs->Name(); },
				[&writer, &outfitter](const Outfit &outfit)
				{
					if(!outfitter->Has(&outfit))
						writer.Write("remove", outfit.Name());
				});
	WriteSorted(outfitter->AsBase(), [](const Outfit *lhs, const Outfit *rhs) { return lhs->Name() < rhs->Name(); },
			[&writer, &diff](const Outfit &outfit)
			{
				if(!diff || !diff->Has(&outfit))
					writer.Write(outfit.Name());
			});
	writer.EndChild();
}
