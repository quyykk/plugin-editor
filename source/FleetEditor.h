// SPDX-License-Identifier: GPL-3.0

#ifndef FLEET_EDITOR_H_
#define FLEET_EDITOR_H_

#include "Fleet.h"
#include "TemplateEditor.h"

#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;



// Class representing the fleet editor window.
class FleetEditor : public TemplateEditor<Fleet> {
public:
	FleetEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const Fleet *fleet) const;

private:
	void RenderFleet();
};



#endif
