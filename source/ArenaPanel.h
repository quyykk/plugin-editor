/* ArenaPanel.h
Copyright (c) 2014 by Michael Zahniser
Copyright (c) 2022 by quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef ARENA_PANEL_H_
#define ARENA_PANEL_H_

#include "Panel.h"

#include "Command.h"
#include "Engine.h"
#include "PlayerInfo.h"
#include "ShipEvent.h"

#include <functional>
#include <list>

class Editor;
class SystemEditor;



// Class representing the main panel (i.e. the view of your ship moving around).
// The goal is that the Engine class will not need to know about displaying
// panels or handling key presses; it instead focuses just on the calculations
// needed to move the ships around and to figure out where they should be drawn.
class ArenaPanel : public Panel {
public:
	static void RenderProperties(SystemEditor &systemEditor, bool &show);

	static inline Date currentDate = Date(16, 11, 3013);
	static inline bool enableFleets = false;
	static inline bool enablePersons = false;
	static inline bool highlightFlagship = false;
	static inline bool showStatusOverlays = false;
	static inline bool showChat = false;


public:
	ArenaPanel(Editor &editor, SystemEditor &systemEditor);

	virtual void Step() override;
	virtual void Draw() override;

	void SetSystem(const System *system);
	void Execute(std::function<void()> f);

	// The main panel allows fast-forward.
	bool AllowsFastForward() const noexcept final;

	Engine &GetEngine() { return engine; }

protected:
	// Only override the ones you need; the default action is to return false.
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool Click(int x, int y, int clicks) override;
	virtual bool RClick(int x, int y) override;
	virtual bool Drag(double dx, double dy) override;
	virtual bool Release(int x, int y) override;
	virtual bool Scroll(double dx, double dy) override;


private:
	void StepEvents(bool &isActive);


private:
	friend class ArenaControl;

	// Things to execute while the engine is paused.
	std::vector<std::function<void()>> commands;

	Editor &editor;
	SystemEditor &systemEditor;

	static inline PlayerInfo player;
	Engine engine;

	Point center;
	bool paused = false;

	// These are the pending ShipEvents that have yet to be processed.
	std::list<ShipEvent> eventQueue;
	bool handledFront = false;

	Command show;

	// For displaying the GPU load.
	double load = 0.;
	double loadSum = 0.;
	int loadCount = 0;

	// Keep track of how long a starting player has spent drifting in deep space.
	int lostness = 0;
	int lostCount = 0;

	Point dragSource;
	Point dragPoint;
	bool isDragging = false;
	bool hasShift = false;
	bool canClick = false;
	bool canDrag = false;

	friend class Editor;
};



#endif
