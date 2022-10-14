// SPDX-License-Identifier: GPL-3.0

#include "MapEditorPanel.h"

#include "text/alignment.hpp"
#include "Angle.h"
#include "CargoHold.h"
#include "Dialog.h"
#include "Editor.h"
#include "FillShader.h"
#include "FogShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "text/Format.h"
#include "Galaxy.h"
#include "Government.h"
#include "Information.h"
#include "LineShader.h"
#include "MainEditorPanel.h"
#include "MapDetailPanel.h"
#include "MapOutfitterPanel.h"
#include "MapShipyardPanel.h"
#include "Mission.h"
#include "MissionPanel.h"
#include "Planet.h"
#include "PlanetEditor.h"
#include "PointerShader.h"
#include "Politics.h"
#include "Preferences.h"
#include "RingShader.h"
#include "Screen.h"
#include "Ship.h"
#include "SpriteShader.h"
#include "StellarObject.h"
#include "SystemEditor.h"
#include "System.h"
#include "Trade.h"
#include "UI.h"

#include "opengl.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

using namespace std;

namespace {
	// Length in frames of the recentering animation.
	const int RECENTER_TIME = 20;
	constexpr int MAX_ZOOM = 3;
	constexpr int MIN_ZOOM = -3;
}



MapEditorPanel::MapEditorPanel(const Editor &editor, PlanetEditor *planetEditor, SystemEditor *systemEditor)
	: editor(editor), planetEditor(planetEditor), systemEditor(systemEditor),
	playerJumpDistance(System::DEFAULT_NEIGHBOR_DISTANCE)
{
	if(!systemEditor->Selected())
		systemEditor->Select(editor.Universe().systems.Get("Sol"));
	selectedSystems.push_back(systemEditor->Selected());

	SetIsFullScreen(true);
	SetInterruptible(false);

	UpdateJumpDistance();

	CenterOnSystem(true);
	UpdateCache();
}



void MapEditorPanel::Step()
{
	if(recentering > 0)
	{
		double step = (recentering - .5) / RECENTER_TIME;
		// Interpolate with the smoothstep function, 3x^2 - 2x^3. Its derivative
		// gives the fraction of the distance to move at each time step:
		center += recenterVector * (step * (1. - step) * (6. / RECENTER_TIME));
		--recentering;
	}
}



void MapEditorPanel::Draw()
{
	glClear(GL_COLOR_BUFFER_BIT);

	for(const auto &it : editor.Universe().galaxies)
		SpriteShader::Draw(it.second.GetSprite(), Zoom() * (center + it.second.Position()), Zoom());

	// Draw the "visible range" circle around your current location.
	Color dimColor(.1f, 0.f);
	RingShader::Draw(Zoom() * (selectedSystems.front()->Position() + center),
		(System::DEFAULT_NEIGHBOR_DISTANCE + .5) * Zoom(), (System::DEFAULT_NEIGHBOR_DISTANCE - .5) * Zoom(), dimColor);
	// Draw the jump range circle around your current location if it is different than the
	// visible range.
	if(playerJumpDistance != System::DEFAULT_NEIGHBOR_DISTANCE)
		RingShader::Draw(Zoom() * (selectedSystems.front()->Position() + center),
			(playerJumpDistance + .5) * Zoom(), (playerJumpDistance - .5) * Zoom(), dimColor);

	Color brightColor(.4f, 0.f);
	for(auto &&system : selectedSystems)
		RingShader::Draw(Zoom() * (system->Position() + center),
			11.f, 9.f, brightColor);

	DrawWormholes();
	DrawLinks();
	DrawSystems();
	DrawNames();

	// Draw the commodity panel.
	Point uiPoint(Screen::Left() + 100., Screen::Top() + 205.);

	tradeY = uiPoint.Y() - 95.;
	const Sprite *tradeSprite = editor.Sprites().Get("ui/map trade");
	SpriteShader::Draw(tradeSprite, uiPoint);
	uiPoint.X() -= 90.;
	uiPoint.Y() -= 97.;

	const System *first = selectedSystems.front();
	const System *second = selectedSystems.back();
	const auto &commodities = editor.Universe().trade.Commodities();
	for(const Trade::Commodity &commodity : commodities)
	{
		const Font &font = FontSet::Get(14);
		const Color &dim = *editor.Universe().colors.Get("dim");
		const Color &medium = *editor.Universe().colors.Get("medium");

		bool isSelected = false;
		if(static_cast<unsigned>(this->commodity) < commodities.size()) 
			isSelected = (&commodity == &commodities[this->commodity]);
		const Color &color = isSelected ? medium : dim;

		font.Draw(commodity.name, uiPoint, color);

		string price;

		int value = second->Trade(commodity.name);
		int localValue = first->Trade(commodity.name);
		if(!value)
			price = "----";
		else if(first == second || !localValue)
			price = to_string(value);
		else
		{
			value -= localValue;
			price += "(";
			if(value > 0)
				price += '+';
			price += to_string(value);
			price += ")";
		}

		const auto alignRight = Layout(140, Alignment::RIGHT, Truncate::BACK);
		font.Draw({price, alignRight}, uiPoint, color);

		if(isSelected)
			PointerShader::Draw(uiPoint + Point(0., 7.), Point(1., 0.), 10.f, 10.f, 0.f, color);

		uiPoint.Y() += 20.;
	}

	// Draw the select drag rectangle.
	if(selectSystems)
	{
		const Color &dragColor = *editor.Universe().colors.Get("drag select");
		LineShader::Draw(dragSource, Point(dragSource.X(), dragPoint.Y()), .8f, dragColor);
		LineShader::Draw(Point(dragSource.X(), dragPoint.Y()), dragPoint, .8f, dragColor);
		LineShader::Draw(dragPoint, Point(dragPoint.X(), dragSource.Y()), .8f, dragColor);
		LineShader::Draw(Point(dragPoint.X(), dragSource.Y()), dragSource, .8f, dragColor);
	}
}



bool MapEditorPanel::AllowsFastForward() const noexcept
{
	return true;
}



const System *MapEditorPanel::Selected() const
{
	return selectedSystems.front();
}



void MapEditorPanel::Select(const Galaxy *galaxy)
{
	center = -galaxy->Position();
}



bool MapEditorPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	if(key == 'f')
	{
		GetUI()->Push(new Dialog(
			this, &MapEditorPanel::Find, "Search for:"));
		return true;
	}
	else if(key == SDLK_PLUS || key == SDLK_KP_PLUS || key == SDLK_EQUALS)
		zoom = min(MAX_ZOOM, zoom + 1);
	else if(key == SDLK_MINUS || key == SDLK_KP_MINUS)
		zoom = max(MIN_ZOOM, zoom - 1);
	else if(key == 'r' && (mod & KMOD_CTRL))
		systemEditor->RandomizeAll();
	else if(key == 't' && (mod & KMOD_CTRL))
		systemEditor->GenerateTrades();
	else if(key == SDLK_DELETE)
		systemEditor->Delete(selectedSystems);
	else if(key == SDLK_DOWN)
	{
		if(commodity < 0 || commodity == 9)
			commodity = 0;
		else
			++commodity;
		UpdateCache();
	}
	else if(key == SDLK_UP)
	{
		if(commodity <= 0)
			commodity = 9;
		else
			--commodity;
		UpdateCache();
	}
	else
		return false;

	return true;
}



bool MapEditorPanel::Click(int x, int y, int clicks)
{
	if(x < Screen::Left() + 160 && y >= tradeY && y < tradeY + 200)
	{
		auto oldCommodity = commodity;
		commodity = (y - tradeY) / 20;
		if(commodity == oldCommodity)
			commodity = -1;
		UpdateCache();
		return true;
	}

	// Figure out if a system was clicked on.
	click = Point(x, y) / Zoom() - center;
	for(const auto &it : editor.Universe().systems)
		if(it.second.IsValid() && click.Distance(it.second.Position()) < 15. / Zoom())
		{
			auto selectedIt = find(selectedSystems.begin(), selectedSystems.end(), &it.second);
			if(selectedIt != selectedSystems.end())
			{
				if(SDL_GetModState() & KMOD_SHIFT)
				{
					// If shift was clicked, we remove the system from the selection but only
					// if this wasn't the only selected system.
					if(selectedSystems.size() > 1)
						selectedSystems.erase(selectedIt);
				}
				else
					moveSystems = true;
			}
			else
				Select(&it.second);

			// On triple click we enter the system.
			if(clicks == 3 && moveSystems)
			{
				GetUI()->Push(editor.SystemViewPanel());
				moveSystems = false;
			}
			return true;
		}

	// If no system was clicked and this was a shift click, we start the select multiple systems
	// drag rectangle.
	if(SDL_GetModState() & KMOD_SHIFT)
	{
		selectSystems = true;
		dragSource = Point(x, y);
		dragPoint = dragSource;
	}
	return true;
}



bool MapEditorPanel::RClick(int x, int y)
{
	rclick = true;
	Point click = Point(x, y) / Zoom() - center;
	for(const auto &it : editor.Universe().systems)
		if(it.second.IsValid() && click.Distance(it.second.Position()) < 10.)
		{
			systemEditor->ToggleLink(&it.second);
			return true;
		}

	// A right click on nothing creates a new system.
	systemEditor->CreateNewSystem(click);
	return true;
}



bool MapEditorPanel::MClick(int x, int y)
{
	Point click = Point(x, y) / Zoom() - center;
	// A middle click on nothing clones the current system.
	systemEditor->CloneSystem(click);
	return true;
}



bool MapEditorPanel::Drag(double dx, double dy)
{
	isDragging = true;
	if(moveSystems)
	{
		for(auto &&system : selectedSystems)
			systemEditor->UpdateSystemPosition(system, Point(dx, dy) / Zoom());
		UpdateCache();
	}
	else if(selectSystems)
		dragPoint += Point(dx, dy);
	else
	{
		center += Point(dx, dy) / Zoom();
		recentering = 0;
	}

	return true;
}



bool MapEditorPanel::Scroll(double dx, double dy)
{
	// The mouse should be pointing to the same map position before and after zooming.
	Point mouse = UI::GetMouse();
	Point anchor = mouse / Zoom() - center;
	if(dy > 0.)
		zoom = min(MAX_ZOOM, zoom + 1);
	else if(dy < 0.)
		zoom = max(MIN_ZOOM, zoom - 1);

	// Now, Zoom() has changed (unless at one of the limits). But, we still want
	// anchor to be the same, so:
	center = mouse / Zoom() - anchor;
	return true;
}



bool MapEditorPanel::Release(int x, int y)
{
	isDragging = false;
	if(moveSystems)
		moveSystems = false;
	if(selectSystems)
	{
		selectSystems = false;

		auto copy = selectedSystems;
		selectedSystems.clear();

		auto rect = Rectangle::WithCorners(dragSource / Zoom() - center, dragPoint / Zoom() - center);
		for(const auto &it : editor.Universe().systems)
			if(it.second.IsValid() && rect.Contains(it.second.Position()))
				Select(&it.second, /*appendSelection=*/true);

		// If no systems were selected then restore the previous selection.
		if(selectedSystems.empty())
			selectedSystems = std::move(copy);
	}

	if(rclick)
	{
		rclick = false;
		return true;
	}
	return true;
}



Color MapEditorPanel::MapColor(double value)
{
	if(std::isnan(value))
		return UninhabitedColor();

	value = min(1., max(-1., value));
	if(value < 0.)
		return Color(
			.12 + .12 * value,
			.48 + .36 * value,
			.48 - .12 * value,
			.4);
	else
		return Color(
			.12 + .48 * value,
			.48,
			.48 - .48 * value,
			.4);
}



Color MapEditorPanel::GovernmentColor(const Government *government)
{
	if(!government)
		return UninhabitedColor();

	return Color(
		.6f * government->GetColor().Get()[0],
		.6f * government->GetColor().Get()[1],
		.6f * government->GetColor().Get()[2],
		.4f);
}



Color MapEditorPanel::UninhabitedColor()
{
	return GovernmentColor(editor.Universe().governments.Get("Uninhabited"));
}



void MapEditorPanel::Select(const System *system, bool appendSelection)
{
	if(!system)
	{
		selectedSystems.clear();
		system = editor.Universe().systems.Get("Sol");
	}
	// Pressing shift selects multiple systems.
	if(!(SDL_GetModState() & KMOD_SHIFT) && !appendSelection)
		selectedSystems.clear();

	selectedSystems.push_back(system);
	systemEditor->Select(selectedSystems.back());
	editor.SystemViewPanel()->Select(selectedSystems.back());
	editor.GetArenaPanel()->SetSystem(selectedSystems.back());
	UpdateJumpDistance();
}



void MapEditorPanel::Find(const string &name)
{
	int bestIndex = 9999;
	for(const auto &it : editor.Universe().systems)
		if(it.second.IsValid())
		{
			int index = Search(it.first, name);
			if(index >= 0 && index < bestIndex)
			{
				bestIndex = index;
				selectedSystems.clear();
				selectedSystems.push_back(&it.second);
				systemEditor->Select(selectedSystems.back());
				editor.SystemViewPanel()->Select(selectedSystems.back());
				editor.GetArenaPanel()->SetSystem(selectedSystems.back());
				CenterOnSystem();
				if(!index)
					return;
			}
		}
	for(const auto &it : editor.Universe().planets)
		if(it.second.IsValid())
		{
			int index = Search(it.first, name);
			if(index >= 0 && index < bestIndex)
			{
				bestIndex = index;
				selectedSystems.clear();
				selectedSystems.push_back(it.second.GetSystem());
				systemEditor->Select(selectedSystems.back());
				editor.SystemViewPanel()->Select(selectedSystems.back());
				editor.GetArenaPanel()->SetSystem(selectedSystems.back());
				CenterOnSystem();
				if(!index)
					return;
			}
		}
}



double MapEditorPanel::Zoom() const
{
	return pow(1.5, zoom);
}



int MapEditorPanel::Search(const string &str, const string &sub)
{
	auto it = search(str.begin(), str.end(), sub.begin(), sub.end(),
		[](char a, char b) { return toupper(a) == toupper(b); });
	return (it == str.end() ? -1 : it - str.begin());
}



void MapEditorPanel::CenterOnSystem(bool immediate)
{
	const auto *system = selectedSystems.back();
	if(immediate)
		center = -system->Position();
	else
	{
		recenterVector = -system->Position() - center;
		recentering = RECENTER_TIME;
	}
}



// Cache the map layout, so it doesn't have to be re-calculated every frame.
// The node cache must be updated when the coloring mode changes.
void MapEditorPanel::UpdateCache()
{
	// Now, update the cache of the links.
	links.clear();

	for(const auto &it : editor.Universe().systems)
	{
		const System *system = &it.second;
		if(!system->IsValid())
			continue;

		for(const System *link : system->Links())
			if(link < system)
			{
				// Only draw links between two systems if both are
				// valid. Also, avoid drawing twice by only drawing in the
				// direction of increasing pointer values.
				if(!link->IsValid())
					continue;

				Link renderLink;
				renderLink.first = system->Position();
				renderLink.second = link->Position();
				// The color of the link is highlighted in red if the trade difference
				// is too big.
				renderLink.color = editor.Universe().colors.Get("map link")->Transparent(.5);
				if(commodity != -1)
				{
					const auto &name = editor.Universe().trade.Commodities()[commodity].name;
					int difference = abs(it.second.Trade(name) - link->Trade(name));
					double value = (difference - 60) / 60.;
					if(value >= 1.)
						renderLink.color = Color(220.f / 255.f, 20.f / 255.f, 60.f / 255.f, 0.5f);
				}

				links.emplace_back(move(renderLink));
			}
	}
}



void MapEditorPanel::UpdateJumpDistance()
{
	// Find out how far the player is able to jump.
	double systemRange = selectedSystems.back()->JumpRange();
	playerJumpDistance = systemRange ? systemRange : System::DEFAULT_NEIGHBOR_DISTANCE;
}



void MapEditorPanel::DrawWormholes()
{
	// Keep track of what arrows and links need to be drawn.
	set<pair<const System *, const System *>> arrowsToDraw;

	// Avoid iterating each StellarObject in every system by iterating over planets instead. A
	// system can host more than one set of wormholes (e.g. Cardea), and some wormholes may even
	// share a link vector. If a wormhole's planet has no description, no link will be drawn.
	for(auto &&it : editor.Universe().planets)
	{
		const Planet &p = it.second;
		if(!p.IsValid() || !p.IsWormhole() || p.Description().empty())
			continue;

		const vector<const System *> &waypoints = p.WormholeSystems();
		const System *from = waypoints.back();
		for(const System *to : waypoints)
		{
			if(from->FindStellar(&p)->HasSprite())
				arrowsToDraw.emplace(from, to);
			from = to;
		}
	}

	const Color &wormholeDim = *editor.Universe().colors.Get("map unused wormhole");
	const Color &arrowColor = *editor.Universe().colors.Get("map used wormhole");
	static const double ARROW_LENGTH = 4.;
	static const double ARROW_RATIO = .3;
	static const Angle LEFT(30.);
	static const Angle RIGHT(-30.);
	const double zoom = Zoom();

	for(const pair<const System *, const System *> &link : arrowsToDraw)
	{
		// Compute the start and end positions of the wormhole link.
		Point from = zoom * (link.first->Position() + center);
		Point to = zoom * (link.second->Position() + center);
		Point offset = (from - to).Unit() * MapPanel::LINK_OFFSET;
		from -= offset;
		to += offset;

		// If an arrow is being drawn, the link will always be drawn too. Draw
		// the link only for the first instance of it in this set.
		if(link.first < link.second || !arrowsToDraw.count(make_pair(link.second, link.first)))
			LineShader::Draw(from, to, MapPanel::LINK_WIDTH, wormholeDim);

		// Compute the start and end positions of the arrow edges.
		Point arrowStem = zoom * ARROW_LENGTH * offset;
		Point arrowLeft = arrowStem - ARROW_RATIO * LEFT.Rotate(arrowStem);
		Point arrowRight = arrowStem - ARROW_RATIO * RIGHT.Rotate(arrowStem);

		// Draw the arrowhead.
		Point fromTip = from - arrowStem;
		LineShader::Draw(from, fromTip, MapPanel::LINK_WIDTH, arrowColor);
		LineShader::Draw(from - arrowLeft, fromTip, MapPanel::LINK_WIDTH, arrowColor);
		LineShader::Draw(from - arrowRight, fromTip, MapPanel::LINK_WIDTH, arrowColor);
	}
}



void MapEditorPanel::DrawLinks()
{
	double zoom = Zoom();
	for(const auto &link : links)
	{
		Point from = zoom * (link.first + center);
		Point to = zoom * (link.second + center);
		Point unit = (from - to).Unit() * MapPanel::LINK_OFFSET;
		from -= unit;
		to += unit;

		LineShader::Draw(from, to, MapPanel::LINK_WIDTH, link.color);
	}
}



void MapEditorPanel::DrawSystems()
{
	// Draw the circles for the systems.
	double zoom = Zoom();
	for(const auto &pair : editor.Universe().systems)
	{
		const auto &system = pair.second;
		if(!system.IsValid())
			continue;

		Point pos = zoom * (system.Position() + center);
		RingShader::Draw(pos, MapPanel::OUTER, MapPanel::INNER, GovernmentColor(system.GetGovernment()));
	}
}



void MapEditorPanel::DrawNames()
{
	// Don't draw if too small.
	double zoom = Zoom();
	if(zoom <= 0.5)
		return;

	// Draw names for all systems you have visited.
	bool useBigFont = (zoom > 2.);
	const Font &font = FontSet::Get(useBigFont ? 18 : 14);
	Point offset(useBigFont ? 8. : 6., -.5 * font.Height());
	for(const auto &pair : editor.Universe().systems)
		font.Draw(pair.second.Name(), zoom * (pair.second.Position() + center) + offset,
				editor.Universe().colors.Get("map name")->Transparent(.75));
}
