// SPDX-License-Identifier: GPL-3.0

#ifndef EFFECT_EDITOR_H_
#define EFFECT_EDITOR_H_

#include "Effect.h"
#include "TemplateEditor.h"

#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;



// Class representing the effect editor window.
class EffectEditor : public TemplateEditor<Effect> {
public:
	EffectEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const Effect *effect) const;

private:
	void RenderPlanetMenu();
	void RenderEffect();
};



#endif
