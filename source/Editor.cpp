/* Editor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Editor.h"

#include "DataFile.h"
#include "DataNode.h"
#include "DataWriter.h"
#include "Dialog.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "Effect.h"
#include "EsUuid.h"
#include "Engine.h"
#include "Files.h"
#include "GameData.h"
#include "GameLoadingPanel.h"
#include "Government.h"
#include "Hazard.h"
#include "ImageSet.h"
#include "MainEditorPanel.h"
#include "MainPanel.h"
#include "MapEditorPanel.h"
#include "MapPanel.h"
#include "Minable.h"
#include "Music.h"
#include "OutfitterEditorPanel.h"
#include "Planet.h"
#include "Ship.h"
#include "Sound.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "TaskQueue.h"
#include "UI.h"
#include "Version.h"
#include "Visual.h"
#include "nfd.h"

#include <SDL2/SDL.h>

#include <cassert>
#include <cstdint>
#include <map>
#include <thread>

using namespace std;



namespace {
	string OpenFileDialog(bool folders, bool openDefault = false)
	{
		string value;
		nfdchar_t *path = nullptr;
		
		auto result = folders ? NFD_PickFolder(&path, openDefault ? Files::Config().data() : nullptr)
			: NFD_OpenDialog(&path, nullptr, 0, openDefault ? Files::Config().data() : nullptr);
		if(result == NFD_OKAY)
		{
			value = path;
			// Convert Windows path separators.
#ifdef _WIN32
			for(char &c : value)
				if(c == '\\')
					c = '/';
#endif
			// Always add a trailing path separator.
			if(value.back() != '/')
				value += '/';
		}
		if(path)
			NFD_FreePath(path);
		return value;
	}
}


Editor::Editor(UI &panels) noexcept
	: effectEditor(*this, showEffectMenu), fleetEditor(*this, showFleetMenu), galaxyEditor(*this, showGalaxyMenu),
	hazardEditor(*this, showHazardMenu),
	governmentEditor(*this, showGovernmentMenu), outfitEditor(*this, showOutfitMenu), outfitterEditor(*this, showOutfitterMenu),
	planetEditor(*this, showPlanetMenu), shipEditor(*this, showShipMenu), shipyardEditor(*this, showShipyardMenu),
	systemEditor(*this, showSystemMenu),
	ui(panels)
{
	StyleColorsGray();
}



void Editor::Initialize()
{
	// Initiale all the panels.
	mapEditorPanel = make_shared<MapEditorPanel>(*this, &planetEditor, &systemEditor);
	mainEditorPanel = make_shared<MainEditorPanel>(*this, &planetEditor, &systemEditor);
	outfitterEditorPanel = make_shared<OutfitterEditorPanel>(*this, shipEditor);

	// The opening panel should be the map editor panel. It is always open and can't be closed.
	ui.Push(mapEditorPanel);

	baseAssets = GameData::Assets().SaveSnapshot();
}



bool Editor::HasPlugin() const
{
	return !currentPluginPath.empty();
}



const std::string &Editor::GetPluginPath() const
{
	return currentPluginPath;
}



const shared_ptr<MapEditorPanel> &Editor::MapPanel() const
{
	return mapEditorPanel;
}



const shared_ptr<MainEditorPanel> &Editor::SystemViewPanel() const
{
	return mainEditorPanel;
}



const shared_ptr<OutfitterEditorPanel> &Editor::OutfitterPanel() const
{
	return outfitterEditorPanel;
}



UI &Editor::GetUI()
{
	return ui;
}



const GameAssets::Snapshot &Editor::BaseUniverse() const
{
	static GameAssets::Snapshot empty;
	return isGameData ? empty : baseAssets;
}



UniverseObjects &Editor::Universe()
{
	return GameData::Assets().objects;
}



const UniverseObjects &Editor::Universe() const
{
	return GameData::Assets().objects;
}



const Set<Sprite> &Editor::Sprites() const
{
	return static_cast<const Set<Sprite> &>(GameData::Assets().sprites);
}



const Set<Sound> &Editor::Sounds() const
{
	return static_cast<const Set<Sound> &>(GameData::Assets().sounds);
}



const SpriteSet &Editor::Spriteset() const
{
	return GameData::Assets().sprites;
}



Plugin &Editor::GetPlugin()
{
	return plugin;
}



void Editor::RenderMain()
{
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	if(!showEditor)
		return;

	if(showEffectMenu)
		effectEditor.Render();
	if(showFleetMenu)
		fleetEditor.Render();
	if(showGalaxyMenu)
		galaxyEditor.Render();
	if(showHazardMenu)
		hazardEditor.Render();
	if(showGovernmentMenu)
		governmentEditor.Render();
	if(showOutfitMenu)
		outfitEditor.Render();
	if(showOutfitterMenu)
		outfitterEditor.Render();
	if(showShipMenu)
		shipEditor.Render();
	if(showShipyardMenu)
		shipyardEditor.Render();
	if(showSystemMenu)
		systemEditor.Render();
	else
		systemEditor.AlwaysRender();
	if(showPlanetMenu)
		planetEditor.Render();
	if(showMainEditorPanelProperties)
		MainEditorPanel::RenderProperties(systemEditor, showMainEditorPanelProperties);
	if(showOutfitterEditorPanelProperties)
		OutfitterEditorPanel::RenderProperties(showOutfitterEditorPanelProperties);

	const bool hasChanges = plugin.HasChanges();

	bool newPluginDialog = false;
	bool openPluginDialog = false;
	bool saveAsPluginDialog = false;
	bool aboutDialog = false;
	bool invalidFolderDialog = false;
	bool invalidGameFolderDialog = false;
	if(ImGui::BeginMainMenuBar())
	{
		if(ImGui::BeginMenu("File"))
		{
			ImGui::MenuItem("New Plugin", "Ctrl+N", &newPluginDialog);
			if(ImGui::BeginMenu("Open"))
			{
				if(ImGui::MenuItem("Plugin")
						&& !OpenPlugin(OpenFileDialog(/*folders=*/true, /*openDefault=*/true)))
					invalidFolderDialog = true;
				if(ImGui::MenuItem("Game Data", "(experimental)")
						&& !OpenGameData(OpenFileDialog(/*folders=*/true, /*openDefault=*/true)))
					invalidGameFolderDialog = true;
				ImGui::EndMenu();
			}
			if(ImGui::MenuItem("Save", "Ctrl+S", false, hasChanges))
			{
				if(HasPlugin())
					SavePlugin();
				else
					saveAsPluginDialog = true;
			}
			ImGui::MenuItem("Save As", "Ctrl+Shift+S", &saveAsPluginDialog, hasChanges);
			if(ImGui::MenuItem("Quit"))
				ShowConfirmationDialog();
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Plugin", HasPlugin()))
			ImGui::EndMenu();
		if(ImGui::BeginMenu("Editors"))
		{
			ImGui::MenuItem("Effect", nullptr, &showEffectMenu);
			ImGui::MenuItem("Fleet", nullptr, &showFleetMenu);
			ImGui::MenuItem("Galaxy", nullptr, &showGalaxyMenu);
			ImGui::MenuItem("Hazard", nullptr, &showHazardMenu);
			ImGui::MenuItem("Government", nullptr, &showGovernmentMenu);
			ImGui::MenuItem("Outfit", nullptr, &showOutfitMenu);
			ImGui::MenuItem("Outfitter", nullptr, &showOutfitterMenu);
			if(ImGui::MenuItem("Ship ", nullptr, &showShipMenu))
				if(ui.Top() != outfitterEditorPanel)
					ui.Push(outfitterEditorPanel);
			ImGui::MenuItem("Shipyard", nullptr, &showShipyardMenu);
			if(ImGui::MenuItem("System", nullptr, &showSystemMenu))
			{
				if(ui.Top() != mapEditorPanel && ui.Top() != mainEditorPanel)
					ui.Push(mapEditorPanel);
			}
			ImGui::MenuItem("Planet", nullptr, &showPlanetMenu);
			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("Views"))
		{
			if(ImGui::MenuItem("Single System"))
				if(ui.Top() != mainEditorPanel)
					ui.Push(mainEditorPanel);
			if(ImGui::MenuItem("Outfitter"))
				if(ui.Top() != outfitterEditorPanel)
					ui.Push(outfitterEditorPanel);
			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("Properties"))
		{
			ImGui::MenuItem("System View", nullptr, &showMainEditorPanelProperties);
			ImGui::MenuItem("Outfitter", nullptr, &showOutfitterEditorPanelProperties);
			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("Themes"))
		{
			if(ImGui::MenuItem("Dark Theme"))
				ImGui::StyleColorsDark();
			if(ImGui::MenuItem("Light Theme"))
				ImGui::StyleColorsLight();
			if(ImGui::MenuItem("Classic Theme"))
				ImGui::StyleColorsClassic();
			if(ImGui::MenuItem("Gray Theme"))
				StyleColorsGray();
			if(ImGui::MenuItem("Dark Gray Theme"))
				StyleColorsDarkGray();
			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("Help"))
		{
			if(ImGui::MenuItem("Wiki"))
				SDL_OpenURL("https://github.com/quyykk/plugin-editor/wiki");
			ImGui::MenuItem("About", nullptr, &aboutDialog);
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	// Handle gloobal shortcuts.
	if(ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N))
		newPluginDialog = true;
	if(hasChanges && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
	{
		if(HasPlugin())
			SavePlugin();
		else
			saveAsPluginDialog = true;
	}

	// Dialogs/popups.
	if(newPluginDialog)
		ImGui::OpenPopup("New Plugin");
	if(openPluginDialog)
		ImGui::OpenPopup("Open Plugin");
	if(saveAsPluginDialog)
		ImGui::OpenPopup("Save As Plugin");
	if(showConfirmationDialog)
		ImGui::OpenPopup("Confirmation Dialog");
	if(aboutDialog)
		ImGui::OpenPopup("About");
	if(invalidFolderDialog)
		ImGui::OpenPopup("Invalid Folder");
	if(invalidGameFolderDialog)
		ImGui::OpenPopup("Invalid Game Folder");

	if(ImGui::BeginPopupModal("New Plugin", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static string newPlugin;
		ImGui::Text("Create new plugin:");
		bool create = ImGui::InputText("##newplugin", &newPlugin, ImGuiInputTextFlags_EnterReturnsTrue);
		if(ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
			newPlugin.clear();
		}
		ImGui::SameLine();
		if(newPlugin.empty())
			ImGui::BeginDisabled();
		bool dontDisable = false;
		if(ImGui::Button("Ok") || create)
		{
			NewPlugin(newPlugin);
			ImGui::CloseCurrentPopup();
			newPlugin.clear();
			dontDisable = true;
		}
		if(newPlugin.empty() && !dontDisable)
			ImGui::EndDisabled();
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopupModal("Open Plugin", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static string openPlugin;
		ImGui::Text("Open plugin:");
		if(ImGui::BeginCombo("##openplugin", openPlugin.c_str()))
		{
			auto plugins = Files::ListDirectories(Files::Config() + "plugins/");
			for(auto plugin : plugins)
			{
				plugin.pop_back();
				plugin = Files::Name(plugin);
				if(ImGui::Selectable(plugin.c_str()))
					openPlugin = plugin;
			}

			ImGui::EndCombo();
		}
		if(ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
			openPlugin.clear();
		}
		ImGui::SameLine();
		if(openPlugin.empty())
			ImGui::BeginDisabled();
		bool dontDisable = false;
		if(ImGui::Button("Ok") || (!openPlugin.empty() && ImGui::IsKeyPressed(ImGuiKey_Enter)))
		{
			OpenPlugin(openPlugin);
			ImGui::CloseCurrentPopup();
			openPlugin.clear();
			dontDisable = true;
		}
		if(openPlugin.empty() && !dontDisable)
			ImGui::EndDisabled();
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopupModal("Save As Plugin", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static string newPlugin;
		ImGui::Text("Save as new plugin:");
		bool create = ImGui::InputText("##newplugin", &newPlugin, ImGuiInputTextFlags_EnterReturnsTrue);
		if(ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
			newPlugin.clear();
		}
		ImGui::SameLine();
		if(newPlugin.empty())
			ImGui::BeginDisabled();
		bool dontDisable = false;
		if(ImGui::Button("Ok") || create)
		{
			NewPlugin(newPlugin, false);
			SavePlugin();
			ImGui::CloseCurrentPopup();
			newPlugin.clear();
			dontDisable = true;
		}
		if(newPlugin.empty() && !dontDisable)
			ImGui::EndDisabled();
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopupModal("Confirmation Dialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("You have unsaved changes. Are you sure you want to quit?");
		if(ImGui::Button("No"))
		{
			showConfirmationDialog = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(ImGui::Button("Yes"))
		{
			ui.Quit();
			showConfirmationDialog = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(!HasPlugin())
			ImGui::BeginDisabled();
		if(ImGui::Button("Save All and Quit"))
		{
			ui.Quit();
			showConfirmationDialog = false;
			SavePlugin();
			ImGui::CloseCurrentPopup();
		}
		if(!HasPlugin())
		{
			ImGui::EndDisabled();
			if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("You don't have a plugin loaded to save the changes to.");
		}
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopup("About", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
	{
		ImGui::Text("Unofficial plugin editor for Endless Sky " ES_VERSION);
		ImGui::Text("created by Nick (@quyykk)");
		ImGui::Text("Built on " EDITOR_DATE);
		ImGui::Spacing();
		ImGui::Text("Report bugs on the ES Discord or on GitHub!");
		if(ImGui::Button("Project link"))
			SDL_OpenURL("https://github.com/quyykk/plugin-editor");
		ImGui::SameLine();
		if(ImGui::Button("ES Discord"))
			SDL_OpenURL("https://discord.gg/ZeuASSx");
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopup("Invalid Folder", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
	{
		ImGui::Text("This folder is not a valid folder! Make sure that you selected the actual plugin folder, and not the 'data' folder inside!");
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopup("Invalid Game Folder", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
	{
		ImGui::Text("This folder is not a valid game folder! Make sure that you selected the actual game folder, and not the 'data' folder inside!");
		ImGui::Text("Also make sure you didn't select a plugin by accident.");
		ImGui::EndPopup();
	}
}



void Editor::ShowConfirmationDialog()
{
	if(HasPlugin() && plugin.HasChanges())
		showConfirmationDialog = true;
	else
		// No unsaved changes, so just quit.
		ui.Quit();
}



void Editor::ResetEditor()
{
	effectEditor.Clear();
	fleetEditor.Clear();
	galaxyEditor.Clear();
	hazardEditor.Clear();
	governmentEditor.Clear();
	outfitEditor.Clear();
	outfitterEditor.Clear();
	shipEditor.Clear();
	shipyardEditor.Clear();
	systemEditor.Clear();
	planetEditor.Clear();
}



void Editor::ResetPanels()
{
	ui.Pop(mapEditorPanel.get());
	ui.Pop(mainEditorPanel.get());
	ui.Pop(outfitterEditorPanel.get());

	mapEditorPanel.reset();
	mainEditorPanel.reset();
	outfitterEditorPanel.reset();
}


void Editor::NewPlugin(const string &plugin, bool reset)
{
	auto pluginsPath = Files::Config() + "plugins/";
	auto plugins = Files::ListDirectories(pluginsPath);

	auto LoadPlugin = [this, &reset, &pluginsPath, &plugin]
	{
		if(reset)
			OpenPlugin(pluginsPath + plugin + "/");
		else
			currentPluginPath = pluginsPath + plugin + "/data/";
	};

	// Don't create a new plugin it if already exists.
	for(const auto &existing : plugins)
		if(existing == plugin)
			return LoadPlugin();

	Files::CreateNewDirectory(pluginsPath + plugin);
	Files::CreateNewDirectory(pluginsPath + plugin + "/data");
	Files::CreateNewDirectory(pluginsPath + plugin + "/images");
	Files::CreateNewDirectory(pluginsPath + plugin + "/sounds");
	LoadPlugin();
}



bool Editor::OpenPlugin(const string &plugin)
{
	if(!Files::Exists(plugin) || !Files::Exists(plugin + "data/"))
		return false;

	currentPluginPath = plugin + "data/";
	isGameData = false;

	// Reset.
	ResetEditor();
	ResetPanels();

	// Revert to the base state.
	GameData::Assets().Revert(baseAssets);

	TaskQueue::Run([this, plugin]
		{
			// Load the plugin.
			GameData::Assets().LoadObjects(currentPluginPath);
			// Find the new images and sounds to load from the plugin.
			GameAssets::SoundMap sounds;
			GameData::Assets().FindSounds(sounds, Files::Resources() + "sounds/");
			GameData::Assets().LoadSounds(plugin + "sounds/", std::move(sounds));

			GameAssets::ImageMap images;
			GameData::Assets().FindImages(images, Files::Resources() + "images/");
			GameData::Assets().LoadSprites(plugin + "images/", std::move(images));

			this->plugin.Load(*this, currentPluginPath);
		});

	showEditor = false;
	ui.Push(new GameLoadingPanel([this](GameLoadingPanel *This)
		{
			ui.Pop(This);

			mapEditorPanel = make_shared<MapEditorPanel>(*this, &planetEditor, &systemEditor);
			mainEditorPanel = make_shared<MainEditorPanel>(*this, &planetEditor, &systemEditor);
			outfitterEditorPanel = make_shared<OutfitterEditorPanel>(*this, shipEditor);

			ui.Push(mapEditorPanel);
		}, showEditor));

	return true;
}



bool Editor::OpenGameData(const string &game)
{
	if(!Files::Exists(game) || !Files::Exists(game + "data/")
			|| !Files::Exists(game + "credits.txt"))
		return false;

	currentPluginPath = game + "data/";
	isGameData = true;

	// Reset.
	ResetEditor();
	ResetPanels();

	// Revert to nothing.
	GameData::Assets().Revert({});

	TaskQueue::Run([this, game]
		{
			// Load the plugin.
			GameData::Assets().LoadObjects(currentPluginPath);
			// Find the new images and sounds to load from the plugin.
			GameAssets::SoundMap sounds;
			GameData::Assets().FindSounds(sounds, Files::Resources() + "sounds/");
			GameData::Assets().LoadSounds(game + "sounds/", std::move(sounds));

			GameAssets::ImageMap images;
			GameData::Assets().FindImages(images, Files::Resources() + "images/");
			GameData::Assets().LoadSprites(game + "images/", std::move(images));

			this->plugin.Load(*this, currentPluginPath);
		});

	showEditor = false;
	ui.Push(new GameLoadingPanel([this](GameLoadingPanel *This)
		{
			ui.Pop(This);

			mapEditorPanel = make_shared<MapEditorPanel>(*this, &planetEditor, &systemEditor);
			mainEditorPanel = make_shared<MainEditorPanel>(*this, &planetEditor, &systemEditor);
			outfitterEditorPanel = make_shared<OutfitterEditorPanel>(*this, shipEditor);

			ui.Push(mapEditorPanel);
		}, showEditor));

	return true;
}



void Editor::SavePlugin()
{
	if(!HasPlugin())
		return;

	plugin.Save(*this, currentPluginPath);
}



void Editor::StyleColorsGray()
{
	// Copyright: OverShifted
	// https://github.com/ocornut/imgui/issues/707#issuecomment-678611331
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_Text]                  = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_ChildBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_Border]                = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.08f, 0.50f, 0.72f, 1.00f);
	style.Colors[ImGuiCol_Button]                = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_Header]                = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.67f, 0.67f, 0.67f, 0.39f);
	style.Colors[ImGuiCol_Separator]             = style.Colors[ImGuiCol_Border];
	style.Colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.41f, 0.42f, 0.44f, 1.00f);
	style.Colors[ImGuiCol_SeparatorActive]       = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.29f, 0.30f, 0.31f, 0.67f);
	style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	style.Colors[ImGuiCol_Tab]                   = ImVec4(0.08f, 0.08f, 0.09f, 0.83f);
	style.Colors[ImGuiCol_TabHovered]            = ImVec4(0.33f, 0.34f, 0.36f, 0.83f);
	style.Colors[ImGuiCol_TabActive]             = ImVec4(0.23f, 0.23f, 0.24f, 1.00f);
	style.Colors[ImGuiCol_TabUnfocused]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
	style.Colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
	style.Colors[ImGuiCol_DockingPreview]        = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
	style.Colors[ImGuiCol_DockingEmptyBg]        = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	style.Colors[ImGuiCol_DragDropTarget]        = ImVec4(0.11f, 0.64f, 0.92f, 1.00f);
	style.Colors[ImGuiCol_NavHighlight]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	style.Colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	style.Colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	style.GrabRounding                           = style.FrameRounding = 2.3f;
}



void Editor::StyleColorsDarkGray()
{
	// Copyright: Raikiri
	// https://github.com/ocornut/imgui/issues/707#issuecomment-512669512
	ImGui::GetStyle().FrameRounding = 4.0f;
	ImGui::GetStyle().GrabRounding = 4.0f;

	ImVec4 *colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}
