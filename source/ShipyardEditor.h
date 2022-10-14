// SPDX-License-Identifier: GPL-3.0

#ifndef SHIPYARD_EDITOR_H_
#define SHIPYARD_EDITOR_H_

#include "Sale.h"
#include "Ship.h"
#include "TemplateEditor.h"

#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;



// Class representing the shipyard editor window.
class ShipyardEditor : public TemplateEditor<Sale<Ship>> {
public:
	ShipyardEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const Sale<Ship> *shipyard) const;

private:
	void RenderShipyard();
};



#endif
