// SPDX-License-Identifier: GPL-3.0

#ifndef OUTFITTER_EDITOR_H_
#define OUTFITTER_EDITOR_H_

#include "Sale.h"
#include "Outfit.h"
#include "TemplateEditor.h"

#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;



// Class representing the outfitter editor window.
class OutfitterEditor : public TemplateEditor<Sale<Outfit>> {
public:
	OutfitterEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const Sale<Outfit> *outfitter) const;

private:
	void RenderOutfitter();
};



#endif
