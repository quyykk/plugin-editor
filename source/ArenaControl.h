/* ArenaControl.h
Copyright (c) 2022 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef ARENA_CONTROL_H_
#define ARENA_CONTROL_H_

#include <memory>

class ArenaPanel;
class Editor;
class SystemEditor;



// Class representing the arena control window.
class ArenaControl {
public:
	ArenaControl(Editor &editor, SystemEditor &systemEditor);

	void SetArena(std::weak_ptr<ArenaPanel> ptr);
	void Render(bool &show);

private:
	Editor &editor;
	SystemEditor &systemEditor;

	std::weak_ptr<ArenaPanel> arena;
};



#endif
