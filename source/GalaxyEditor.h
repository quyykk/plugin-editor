/* GalaxyEditor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef GALAXY_EDITOR_H_
#define GALAXY_EDITOR_H_

#include "Galaxy.h"
#include "TemplateEditor.h"

class DataWriter;
class Editor;



// Class representing the galaxy editor window.
class GalaxyEditor : public TemplateEditor<Galaxy> {
public:
	GalaxyEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const Galaxy *galaxy) const;

private:
	void RenderGalaxy();
};



#endif
