/* SystemEditor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef SYSTEM_EDITOR_H_
#define SYSTEM_EDITOR_H_

#include "System.h"
#include "TemplateEditor.h"

#include <random>
#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;
class MainEditorPanel;
class MapEditorPanel;
class StellarObject;



// Class representing the system editor window.
class SystemEditor : public TemplateEditor<System> {
public:
	SystemEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void AlwaysRender(bool showNewSystem = false, bool showCloneSystem = false);
	void WriteToFile(DataWriter &writer, const System *system) const;

	void Delete(const std::vector<const System *> &systems);

	// Updates the given system's position by the given delta.
	void UpdateSystemPosition(const System *system, Point dp);
	// Updates the given stellar's position by the given delta.
	void UpdateStellarPosition(const StellarObject &object, Point pos, const System *system);
	// Standardizes periods, etc. of the system's stellar objects.
	void StandardizeSystem();
	// Toggles a link between the current object and the given one.
	void ToggleLink(const System *system);
	// Create a new system at the specified position.
	void CreateNewSystem(Point position);
	// Clones a new system at the specified position.
	void CloneSystem(Point position);
	void Delete(const StellarObject &stellar, bool selectedObject);

	const System *Selected() const { return object; }
	void Select(const System *system) { object = const_cast<System *>(system); selectedObject = nullptr; }
	void Select(const StellarObject *object) { selectedObject = object; }

	void RandomizeAll();
	void GenerateTrades();

	void UpdateMap() const;
	void UpdateMain() const;


private:
	void RenderSystem();
	void RenderObject(StellarObject &object, int index, int &nested, bool &hovered, bool &add);
	void RenderHazards(std::vector<RandomEvent<Hazard>> &hazards);

	void WriteObject(DataWriter &writer, const System *system, const StellarObject *object, bool add = false) const;

	void Randomize();
	void RandomizeAsteroids();
	void RandomizeMinables();

	const Sprite *RandomStarSprite();
	const Sprite *RandomPlanetSprite(bool recalculate = false);
	const Sprite *RandomMoonSprite();
	const Sprite *RandomGiantSprite();

	void Delete(const System *system, bool safe);

private:
	Point position;
	bool createNewSystem = false;
	bool cloneSystem = false;
	const StellarObject *selectedObject = nullptr;

	std::random_device rd;
	std::mt19937 gen;
};



#endif
