// SPDX-License-Identifier: GPL-3.0

#ifndef SHIP_EDITOR_H_
#define SHIP_EDITOR_H_

#include "Ship.h"
#include "TemplateEditor.h"

#include <memory>
#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;



// Class representing the ship editor window.
class ShipEditor : public TemplateEditor<Ship> {
public:
	ShipEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const Ship *ship) const;

	Ship *GetShip() { return object; }
	void SetModified() { SetDirty(); }


private:
	void RenderShip();
	void RenderHardpoint();
};



#endif
