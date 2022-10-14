// SPDX-License-Identifier: GPL-3.0

#ifndef OUTFIT_EDITOR_H_
#define OUTFIT_EDITOR_H_

#include "Outfit.h"
#include "TemplateEditor.h"

#include <array>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class DataWriter;
class Editor;



// Class representing the outfit editor window.
class OutfitEditor : public TemplateEditor<Outfit> {
public:
	static const std::array<std::string_view, 182> &AttributesOrder();



public:
	OutfitEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const Outfit *outfit) const;

private:
	void RenderOutfitMenu();
	void RenderOutfit();
};



#endif
