/* MainEditorPanel.cpp
Copyright (c) 2014 by Michael Zahniser
Copyright (c) 2021 by quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "MainEditorPanel.h"

#include "text/alignment.hpp"
#include "Angle.h"
#include "CargoHold.h"
#include "Dialog.h"
#include "Editor.h"
#include "FillShader.h"
#include "Flotsam.h"
#include "FogShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "text/Format.h"
#include "Galaxy.h"
#include "Government.h"
#include "Information.h"
#include "Interface.h"
#include "LineShader.h"
#include "MapDetailPanel.h"
#include "MapOutfitterPanel.h"
#include "MapShipyardPanel.h"
#include "Mission.h"
#include "MissionPanel.h"
#include "Planet.h"
#include "PlanetEditor.h"
#include "PointerShader.h"
#include "Politics.h"
#include "Radar.h"
#include "RingShader.h"
#include "Screen.h"
#include "Ship.h"
#include "SpriteShader.h"
#include "StarField.h"
#include "StellarObject.h"
#include "SystemEditor.h"
#include "System.h"
#include "Trade.h"
#include "UI.h"
#include "Visual.h"

#include "opengl.h"

#include <SDL_keycode.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <imgui.h>
#include <limits>

using namespace std;


namespace {
	constexpr double ZOOMS[] = {.1, .15, .2, .25, .35, .50, .70, 1., 1.4, 2.};
	constexpr size_t SIZE = sizeof(ZOOMS) / sizeof(double);
}



void MainEditorPanel::RenderProperties(SystemEditor &systemEditor, bool &show)
{
	if(!ImGui::Begin("In-System Properties", &show, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	ImGui::Checkbox("Show Habitable Rings", &showHabitableRings);
	ImGui::Checkbox("Show Orbits", &showOrbits);
	ImGui::Checkbox("Show Belts", &showBelts);
	ImGui::Checkbox("Show Arrival Distance", &showArrivalDistance);
	ImGui::InputInt("Time Increment", &timeIncrement);
	ImGui::End();
}



MainEditorPanel::MainEditorPanel(const Editor &editor, PlanetEditor *planetEditor, SystemEditor *systemEditor)
	: planetEditor(planetEditor), systemEditor(systemEditor)
{
	zoom = ViewZoom();
	if(!systemEditor->Selected())
		systemEditor->Select(editor.Universe().systems.Get("Sol"));
	Select(systemEditor->Selected());
	SetIsFullScreen(true);
	SetInterruptible(false);

	UpdateSystem();
	UpdateCache();
}



void MainEditorPanel::Step()
{
	UpdateSystem();

	double zoomTarget = ViewZoom();
	if(zoom != zoomTarget)
	{
		static const double ZOOM_SPEED = .05;

		// Define zoom speed bounds to prevent asymptotic behavior.
		static const double MAX_SPEED = .05;
		static const double MIN_SPEED = .002;
		const Point anchor = -mouse / zoom - center;

		double zoomRatio = max(MIN_SPEED, min(MAX_SPEED, abs(log2(zoom) - log2(zoomTarget)) * ZOOM_SPEED));
		if(zoom < zoomTarget)
			zoom = min(zoomTarget, zoom * (1. + zoomRatio));
		else if(zoom > zoomTarget)
			zoom = max(zoomTarget, zoom * (1. / (1. + zoomRatio)));
		center = -mouse / zoom - anchor;
	}

	labels.clear();
	for(const StellarObject &object : currentSystem->Objects())
		if(object.planet)
			labels.emplace_back(object.Position() - center, object, currentSystem, zoom, true);
}



void MainEditorPanel::Draw()
{
	glClear(GL_COLOR_BUFFER_BIT);
	GameData::Background().Draw(center, Point(), zoom);
	for(const PlanetLabel &label : labels)
		label.Draw();

	draw.Clear(step, zoom);
	batchDraw.Clear(step, zoom);
	draw.SetCenter(center);
	batchDraw.SetCenter(center);

	for(const auto &object : currentSystem->Objects())
	{
		if(object.HasSprite())
		{
			// Don't apply motion blur to very large planets and stars.
			if(object.Width() >= 280.)
				draw.AddUnblurred(object);
			else
				draw.Add(object);
		}

		if(object.IsStar() || !showOrbits)
			continue;

		if(object.Parent() == -1)
			RingShader::Draw(-center * zoom, object.Distance() * zoom, object.Radius() * zoom, 1.f, Color(169.f / 255.f, 169.f / 255.f, 169.f / 255.f).Transparent(.1f));
		else
		{
			const auto &parent = currentSystem->Objects()[object.Parent()];
			RingShader::Draw((parent.Position() - center) * zoom,
					object.Distance() * zoom, object.Radius() * zoom, 1.f,
					Color(169.f / 255.f, 169.f / 255.f, 169.f / 255.f).Transparent(.1f));
		}
	}

	asteroids.Step(newVisuals, newFlotsam, step);
	asteroids.Draw(draw, center, zoom);
	for(const auto &visual : newVisuals)
		batchDraw.AddVisual(visual);
	for(const auto &floatsam : newFlotsam)
		draw.Add(*floatsam);
	++step;

	if(currentObject)
	{
		Angle a = currentObject->Facing();
		Angle da(360. / 5);

		PointerShader::Bind();
		for(int i = 0; i < 5; ++i)
		{
			PointerShader::Add((currentObject->Position() - center) * zoom, a.Unit(), 12.f, 14.f, -currentObject->RealRadius() * zoom, Radar::GetColor(Radar::FRIENDLY));
			a += da;
		}
		PointerShader::Unbind();
	}

	if(showHabitableRings)
	{
		RingShader::Draw(-center * zoom, currentSystem->HabitableZone() * zoom, 2.5f, 1.f, Color(50.f / 255.f, 205.f / 255.f, 50.f / 255.f).Transparent(.3f));
		RingShader::Draw(-center * zoom, currentSystem->HabitableZone() * .5 * zoom, 2.5f, 1.f, Color(1.f, 140.f / 255.f, 0.f).Transparent(.3f));
		RingShader::Draw(-center * zoom, currentSystem->HabitableZone() * 2 * zoom, 2.5f, 1.f, Color(0.f, 191.f / 255.f, 1.f).Transparent(.3f));
	}

	if(showBelts)
		for(const auto &belt : currentSystem->AsteroidBelts())
			RingShader::Draw(-center * zoom, belt.item * zoom, 2.5f, 1.f, Color(230.f / 255.f, 176.f / 255.f, 170.f / 255.f).Transparent(.3f));

	if(showArrivalDistance)
	{
		RingShader::Draw(-center * zoom, currentSystem->ExtraHyperArrivalDistance() * zoom, 2.5f, 1.f, Color(243.f / 255.f, 156.f / 255.f, 18.f / 255.f).Transparent(.3f));
		RingShader::Draw(-center * zoom, currentSystem->ExtraJumpArrivalDistance() * zoom, 2.5f, 1.f, Color(222.f / 255.f, 49.f / 255.f, 99.f / 255.f).Transparent(.3f));
	}
	draw.Draw();
	batchDraw.Draw();
}



bool MainEditorPanel::AllowsFastForward() const noexcept
{
	return true;
}



const System *MainEditorPanel::Selected() const
{
	return currentSystem;
}



void MainEditorPanel::DeselectObject()
{
	currentObject = nullptr;
}



void MainEditorPanel::SelectObject(const StellarObject &stellar)
{
	currentObject = &stellar;
}



bool MainEditorPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	if(command.Has(Command::MAP) || key == 'd' || key == SDLK_ESCAPE
			|| (key == 'w' && (mod & (KMOD_CTRL | KMOD_GUI))))
		GetUI()->Pop(this);
	else if(key == SDLK_PLUS || key == SDLK_KP_PLUS || key == SDLK_EQUALS)
		ZoomViewIn();
	else if(key == SDLK_MINUS || key == SDLK_KP_MINUS)
		ZoomViewOut();
	else if(key == SDLK_DELETE && currentObject)
		systemEditor->Delete(*currentObject, true);
	else if(key == 'r' && (mod & KMOD_CTRL))
		systemEditor->RandomizeAll();
	else if(key == SDLK_SPACE)
		paused = !paused;
	else
		return false;

	return true;
}



bool MainEditorPanel::Click(int x, int y, int clicks)
{
	// Figure out if a system was clicked on.
	click = Point(x, y) / zoom + center;
	if(!currentSystem || !currentSystem->IsValid())
		return false;
	double dist = numeric_limits<double>::max();
	const StellarObject *selected = nullptr;
	for(const auto &it : currentSystem->Objects())
	{
		const double clickDist = click.Distance(it.Position());
		if(clickDist < it.RealRadius() && clickDist < dist)
		{
			dist = clickDist;
			selected = &it;
		}
	}
	if(selected)
	{
		currentObject = selected;
		systemEditor->Select(currentObject);
		if(currentObject->planet)
			planetEditor->Select(currentObject->planet);
		moveStellars = true;
		return true;
	}

	systemEditor->Select(currentObject = nullptr);
	moveStellars = false;
	return true;
}



bool MainEditorPanel::Drag(double dx, double dy)
{
	isDragging = true;
	if(moveStellars && currentObject)
	{
		click += Point(dx, dy) / zoom;
		auto dragCenter = currentObject->parent == -1 ? Point()
			: currentSystem->objects[currentObject->parent].position;
		systemEditor->UpdateStellarPosition(*currentObject, click - dragCenter, currentSystem);
	}
	else
		center -= Point(dx, dy) / zoom;

	return true;
}



bool MainEditorPanel::Scroll(double dx, double dy)
{
	if(dy < 0.)
		ZoomViewOut();
	else if(dy > 0.)
		ZoomViewIn();
	mouse = UI::GetMouse();
	return true;
}



bool MainEditorPanel::Release(int x, int y)
{
	// Figure out if a system was clicked on, but
	// only if we didn't move the systems.
	if(isDragging)
	{
		isDragging = false;
		moveStellars = false;
		return true;
	}

	return true;
}



void MainEditorPanel::Select(const System *system)
{
	if(!system)
		return;
	currentSystem = system;
	currentObject = nullptr;
	UpdateCache();
}



void MainEditorPanel::UpdateSystem()
{
	if(!paused)
		date += timeIncrement;
	double now = date / 60.;
	auto &objects = const_cast<System *>(currentSystem)->objects;
	for(StellarObject &object : objects)
	{
		// "offset" is used to allow binary orbits; the second object is offset
		// by 180 degrees.
		object.angle = Angle(now * object.speed + object.offset);
		object.position = object.angle.Unit() * object.distance;

		// Because of the order of the vector, the parent's position has always
		// been updated before this loop reaches any of its children, so:
		if(object.parent >= 0)
			object.position += objects[object.parent].position;

		if(object.position)
			object.angle = Angle(object.position);
	}
}



void MainEditorPanel::UpdateCache()
{
	asteroids.Clear();
	for(const System::Asteroid &a : currentSystem->Asteroids())
	{
		// Check whether this is a minable or an ordinary asteroid.
		if(a.Type())
			asteroids.Add(a.Type(), a.Count(), a.Energy(), currentSystem->AsteroidBelts());
		else
			asteroids.Add(a.Name(), a.Count(), a.Energy());
	}
}



double MainEditorPanel::ViewZoom() const
{
	return ZOOMS[zoomIndex];
}



void MainEditorPanel::ZoomViewIn()
{
	if(zoomIndex < SIZE - 1)
		++zoomIndex;
}



void MainEditorPanel::ZoomViewOut()
{
	if(zoomIndex > 0)
		--zoomIndex;
}
