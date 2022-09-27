/* Editor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef EDITOR_H_
#define EDITOR_H_

#include "GameAssets.h"

#include "EffectEditor.h"
#include "FleetEditor.h"
#include "GalaxyEditor.h"
#include "HazardEditor.h"
#include "GovernmentEditor.h"
#include "OutfitEditor.h"
#include "OutfitterEditor.h"
#include "PlanetEditor.h"
#include "ShipEditor.h"
#include "ShipyardEditor.h"
#include "SystemEditor.h"

#include "Plugin.h"
#include "UniverseObjects.h"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class Body;
class Engine;
class OutfitterEditorPanel;
class Sprite;
class StellarObject;
class UI;



class HashPairOfStrings
{
public:
	std::size_t operator()(const std::pair<std::string, std::string> &pair) const noexcept
	{
		return std::hash<std::string>()(pair.first) * 3 + std::hash<std::string>()(pair.second);
	}
};



// Class representing the editor UI.
class Editor {
public:
	Editor(UI &panels) noexcept;
	Editor(const Editor &) = delete;
	Editor &operator=(const Editor &) = delete;

	void Initialize();

	void RenderMain();

	void ShowConfirmationDialog();

	bool HasPlugin() const;
	const std::string &GetPluginPath() const;
	UI &GetUI();

	const GameAssets::Snapshot &BaseUniverse() const;
	UniverseObjects &Universe();
	const UniverseObjects &Universe() const;
	const Set<Sprite> &Sprites() const;
	const Set<Sound> &Sounds() const;
	const SpriteSet &Spriteset() const;
	Plugin &GetPlugin();

	const std::shared_ptr<MapEditorPanel> &MapPanel() const;
	const std::shared_ptr<MainEditorPanel> &SystemViewPanel() const;
	const std::shared_ptr<OutfitterEditorPanel> &OutfitterPanel() const;


private:
	void ResetEditor();
	void ResetPanels();

	void NewPlugin(const std::string &plugin, bool reset = true);
	void OpenPlugin(const std::string &plugin);
	void OpenGameData(const std::string &game);
	void SavePlugin();

	void StyleColorsGray();
	void StyleColorsDarkGray();


public:
	EffectEditor effectEditor;
	FleetEditor fleetEditor;
	GalaxyEditor galaxyEditor;
	HazardEditor hazardEditor;
	GovernmentEditor governmentEditor;
	OutfitEditor outfitEditor;
	OutfitterEditor outfitterEditor;
	PlanetEditor planetEditor;
	ShipEditor shipEditor;
	ShipyardEditor shipyardEditor;
	SystemEditor systemEditor;


private:
	UI &ui;
	bool showEditor = true;

	// The base universe of the game without any plugins.
	GameAssets::Snapshot baseAssets;

	std::shared_ptr<MapEditorPanel> mapEditorPanel;
	std::shared_ptr<MainEditorPanel> mainEditorPanel;
	std::shared_ptr<OutfitterEditorPanel> outfitterEditorPanel;

	Plugin plugin;
	std::string currentPluginPath;
	bool isGameData = false;

	bool showConfirmationDialog = false;
	bool showMainEditorPanelProperties = false;
	bool showOutfitterEditorPanelProperties = false;

	bool showEffectMenu = false;
	bool showFleetMenu = false;
	bool showGalaxyMenu = false;
	bool showHazardMenu = false;
	bool showGovernmentMenu = false;
	bool showOutfitMenu = false;
	bool showOutfitterMenu = false;
	bool showShipMenu = false;
	bool showShipyardMenu = false;
	bool showSystemMenu = false;
	bool showPlanetMenu = false;
};



#endif
