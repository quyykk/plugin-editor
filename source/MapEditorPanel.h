/* MapEditorPanel.h
Copyright (c) 2014 by Michael Zahniser
Copyright (c) 2021 by quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef MAP_EDITOR_PANEL_H_
#define MAP_EDITOR_PANEL_H_

#include "Panel.h"

#include "Color.h"
#include "Point.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

class Angle;
class Editor;
class Galaxy;
class Government;
class PlanetEditor;
class System;
class SystemEditor;



// This class provides a limited version of the MapPanel which you can edit.
class MapEditorPanel : public Panel {
public:
	MapEditorPanel(const Editor &editor, PlanetEditor *planetEditor, SystemEditor *systemEditor);

	virtual void Step() override;
	virtual void Draw() override;

	// Map panels allow fast-forward to stay active.
	virtual bool AllowsFastForward() const noexcept override;

	const System *Selected() const;
	void Select(const Galaxy *galaxy);


protected:
	// Only override the ones you need; the default action is to return false.
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool Click(int x, int y, int clicks) override;
	virtual bool RClick(int x, int y) override;
	virtual bool MClick(int x, int y) override;
	virtual bool Drag(double dx, double dy) override;
	virtual bool Scroll(double dx, double dy) override;
	virtual bool Release(int x, int y) override;

	// Get the color mapping for various system attributes.
	Color MapColor(double value);
	Color GovernmentColor(const Government *government);
	Color UninhabitedColor();

	void Select(const System *system, bool appendSelection = false);
	void Find(const std::string &name);

	double Zoom() const;

	// Function for the "find" dialogs:
	static int Search(const std::string &str, const std::string &sub);


protected:
	const Editor &editor;
	PlanetEditor *planetEditor;
	SystemEditor *systemEditor;

	// The (non-null) system which is currently selected.
	std::vector<const System *> selectedSystems;

	double playerJumpDistance;

	Point center;
	Point recenterVector;
	int recentering = 0;

	// Center the view on the given system (may actually be slightly offset
	// to account for panels on the screen).
	void CenterOnSystem(bool immediate = false);

	// Cache the map layout, so it doesn't have to be re-calculated every frame.
	// The cache must be updated when the coloring mode changes.
	void UpdateCache();

	void UpdateJumpDistance();


private:
	void DrawWormholes();
	void DrawLinks();
	// Draw systems in accordance to the set commodity color scheme.
	void DrawSystems();
	void DrawNames();


private:
	struct Link {
		Point first;
		Point second;
		Color color;
	};
	std::vector<Link> links;
	Point click;
	bool isDragging = false;
	bool rclick = false;
	bool moveSystems = false;

	bool selectSystems = false;
	Point dragSource;
	Point dragPoint;

	int commodity = -1;
	int tradeY = 0;

	int zoom = 0;

	friend class SystemEditor;
};



#endif
