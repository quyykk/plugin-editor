// SPDX-License-Identifier: GPL-3.0

#ifndef HAZARD_EDITOR_H_
#define HAZARD_EDITOR_H_

#include "Hazard.h"
#include "TemplateEditor.h"

#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;



// Class representing the hazard editor window.
class HazardEditor : public TemplateEditor<Hazard> {
public:
	HazardEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const Hazard *hazard) const;

private:
	void RenderHazard();
};



#endif
