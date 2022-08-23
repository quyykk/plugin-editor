/* MainEditorPanel.h
Copyright (c) 2014 by Michael Zahniser
Copyright (c) 2021 by quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef MAIN_EDITOR_PANEL_H_
#define MAIN_EDITOR_PANEL_H_

#include "Panel.h"

#include "AsteroidField.h"
#include "BatchDrawList.h"
#include "Date.h"
#include "DrawList.h"
#include "Color.h"
#include "PlanetLabel.h"
#include "Point.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

class Angle;
class Editor;
class Government;
class PlanetEditor;
class StellarObject;
class System;
class SystemEditor;



// This class provides a limited version of the MainPanel which you can edit.
class MainEditorPanel : public Panel {
public:
	static void RenderProperties(SystemEditor &systemEditor, bool &show);

	static inline bool showHabitableRings = true;
	static inline bool showOrbits = true;
	static inline bool showBelts = false;
	static inline bool showArrivalDistance = false;
	static inline int timeIncrement = 1;


public:
	MainEditorPanel(const Editor &editor, PlanetEditor *planetEditor, SystemEditor *systemEditor);

	virtual void Step() override;
	virtual void Draw() override;

	// Map panels allow fast-forward to stay active.
	virtual bool AllowsFastForward() const noexcept override;

	void Select(const System *system);
	const System *Selected() const;
	void DeselectObject();
	void SelectObject(const StellarObject &stellar);
	const StellarObject *SelectedObject() { return currentObject; }


protected:
	// Only override the ones you need; the default action is to return false.
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool Click(int x, int y, int clicks) override;
	virtual bool Drag(double dx, double dy) override;
	virtual bool Scroll(double dx, double dy) override;
	virtual bool Release(int x, int y) override;


protected:
	PlanetEditor *planetEditor;
	SystemEditor *systemEditor;

	// The (non-null) system which is currently selected.
	const System *currentSystem;
	const StellarObject *currentObject = nullptr;
	AsteroidField asteroids;
	std::list<std::shared_ptr<Flotsam>> newFlotsam;
	std::vector<Visual> newVisuals;

	Point center;
	int step = 0;
	double zoom;
	Point mouse;

	void UpdateSystem();
	void UpdateCache();

	double ViewZoom() const;
	void ZoomViewIn();
	void ZoomViewOut();


private:
	size_t zoomIndex = 4;
	int64_t date = 0;
	bool paused = false;

	Point click;

	bool isDragging = false;
	bool moveStellars = false;
	DrawList draw;
	BatchDrawList batchDraw;
	std::vector<PlanetLabel> labels;

	friend class SystemEditor;
};



#endif
