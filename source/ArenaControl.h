// SPDX-License-Identifier: GPL-3.0

#ifndef ARENA_CONTROL_H_
#define ARENA_CONTROL_H_

#include <memory>

class ArenaPanel;
class Editor;
class Government;
class Ship;
class SystemEditor;



// Class representing the arena control window.
class ArenaControl {
public:
	ArenaControl(Editor &editor, SystemEditor &systemEditor);

	void SetArena(std::weak_ptr<ArenaPanel> ptr);
	void Render(bool &show);


private:
	void PlaceShip(const Ship &ship, const Government &gov) const;


private:
	Editor &editor;
	SystemEditor &systemEditor;

	std::weak_ptr<ArenaPanel> arena;
};



#endif
