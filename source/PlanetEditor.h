// SPDX-License-Identifier: GPL-3.0

#ifndef PLANET_EDITOR_H_
#define PLANET_EDITOR_H_

#include "Planet.h"
#include "TemplateEditor.h"

#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;



// Class representing the planet editor window.
class PlanetEditor : public TemplateEditor<Planet> {
public:
	PlanetEditor(Editor &editor, bool &show) noexcept;

	void Select(const Planet *planet);

	void Render();
	void WriteToFile(DataWriter &writer, const Planet *planet) const;

private:
	void RenderPlanetMenu();
	void RenderPlanet();
};



#endif
