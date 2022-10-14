// SPDX-License-Identifier: GPL-3.0

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
