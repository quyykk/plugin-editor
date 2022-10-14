// SPDX-License-Identifier: GPL-3.0

#include "ArenaPanel.h"

#include "comparators/ByGivenOrder.h"
#include "BoardingPanel.h"
#include "Dialog.h"
#include "Editor.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "text/Format.h"
#include "FrameTimer.h"
#include "GameData.h"
#include "Government.h"
#include "HailPanel.h"
#include "MapDetailPanel.h"
#include "Messages.h"
#include "Mission.h"
#include "Phrase.h"
#include "Planet.h"
#include "PlanetPanel.h"
#include "PlayerInfo.h"
#include "PlayerInfoPanel.h"
#include "Preferences.h"
#include "Projectile.h"
#include "Random.h"
#include "Screen.h"
#include "Ship.h"
#include "ShipEvent.h"
#include "StellarObject.h"
#include "System.h"
#include "UI.h"
#include "Visual.h"
#include "Weather.h"

#include "opengl.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

using namespace std;



void ArenaPanel::RenderProperties(SystemEditor &systemEditor, bool &show)
{
	if(!ImGui::Begin("Arena Properties", &show, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	int date[3] = {currentDate.Day(), currentDate.Month(), currentDate.Year()};
	if(ImGui::InputInt3("Current Date", date))
	{
		date[0] = clamp(date[0], 1, 31);
		date[1] = clamp(date[1], 1, 12);
		date[2] = clamp(date[2], 1, 8388607 /*2^23 - 1*/);
		currentDate = Date(date[0], date[1], date[2]);

		player.date = currentDate;
		if(systemEditor.Selected())
			const_cast<System *>(systemEditor.Selected())->SetDate(currentDate);
	}

	if(ImGui::Checkbox("Enable Fleet Spawns", &enableFleets))
		Preferences::Set("editor - fleet spawn", enableFleets);
	if(ImGui::Checkbox("Enable Person Ship Spawns", &enablePersons))
		Preferences::Set("editor - person spawn", enablePersons);
	if(ImGui::Checkbox("Highlight Flagship", &highlightFlagship))
		Preferences::Set("flagship highlight", highlightFlagship);
	if(ImGui::Checkbox("Show Status Overlays", &showStatusOverlays))
		Preferences::Set("Show status overlays", showStatusOverlays);
	if(ImGui::Checkbox("Show Chat", &showChat))
		Preferences::Set("editor - show chat", showChat);

	ImGui::End();
}



ArenaPanel::ArenaPanel(Editor &editor, SystemEditor &systemEditor)
	: editor(editor), systemEditor(systemEditor), engine(player)
{
	SetIsFullScreen(true);

	player.date = currentDate;
	if(systemEditor.Selected())
		systemEditor.Select(editor.Universe().systems.Get("Sol"));
	SetSystem(systemEditor.Selected());

	highlightFlagship = Preferences::Has("flagship highlight");
	showStatusOverlays = Preferences::Has("Show status overlays");
	enableFleets = Preferences::Has("editor - fleet spawn");
	enablePersons = Preferences::Has("editor - person spawn");
	showChat = Preferences::Has("editor - show chat");
}



void ArenaPanel::Step()
{
	if(GetUI()->Top().get() != this)
		return;
	// Update the planets' dates.
	if(player.GetSystem())
		const_cast<System *>(player.GetSystem())->SetDate(currentDate);

	engine.Wait();

	// Depending on what UI element is on top, the game is "paused." This
	// checks only already-drawn panels.
	bool isActive = !paused && GetUI()->IsTop(this);

	engine.Step(isActive);

	// Execute any commands.
	for(const auto &f : commands)
		f();
	commands.clear();

	// Splice new events onto the eventQueue for (eventual) handling. No
	// other classes use Engine::Events() after Engine::Step() completes.
	eventQueue.splice(eventQueue.end(), engine.Events());
	// Handle as many ShipEvents as possible (stopping if no longer active
	// and updating the isActive flag).
	StepEvents(isActive);

	if(isActive)
		engine.Go();
	else
		canDrag = false;
	canClick = isActive;
}



void ArenaPanel::Draw()
{
	FrameTimer loadTimer;
	glClear(GL_COLOR_BUFFER_BIT);

	engine.Draw();

	if(Preferences::Has("Show CPU / GPU load"))
	{
		string loadString = to_string(lround(load * 100.)) + "% GPU";
		const Color &color = *GameData::Colors().Get("medium");
		FontSet::Get(14).Draw(loadString, Point(10., Screen::Height() * -.5 + 5.), color);

		loadSum += loadTimer.Time();
		if(++loadCount == 60)
		{
			load = loadSum;
			loadSum = 0.;
			loadCount = 0;
		}
	}
}



void ArenaPanel::SetSystem(const System *system)
{
	Execute([this, system] {
		if(system)
			player.SetSystem(*system);
		const_cast<System *>(system)->SetDate(currentDate);
		engine.visuals.clear();
		engine.projectiles.clear();
		engine.flotsam.clear();
		engine.activeWeather.clear();
		engine.ships.clear();
	});
}



void ArenaPanel::Execute(function<void()> f)
{
	commands.push_back(std::move(f));
}



bool ArenaPanel::AllowsFastForward() const noexcept
{
	return true;
}



// Only override the ones you need; the default action is to return false.
bool ArenaPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	if(command.Has(Command::MAP) || key == 'd' || key == SDLK_ESCAPE
			|| (key == 'w' && (mod & (KMOD_CTRL | KMOD_GUI))))
		GetUI()->Pop(this);
	else if(command.Has(Command::MAP | Command::INFO | Command::HAIL))
		show = command;
	else if(command.Has(Command::AMMO))
	{
		Preferences::ToggleAmmoUsage();
		Messages::Add("Your escorts will now expend ammo: " + Preferences::AmmoUsage() + "."
			, Messages::Importance::High);
	}
	else if((key == SDLK_MINUS || key == SDLK_KP_MINUS) && !command)
		Preferences::ZoomViewOut();
	else if((key == SDLK_PLUS || key == SDLK_KP_PLUS || key == SDLK_EQUALS) && !command)
		Preferences::ZoomViewIn();
	else if(key >= '0' && key <= '9' && !command)
		engine.SelectGroup(key - '0', mod & KMOD_SHIFT, mod & (KMOD_CTRL | KMOD_GUI));
	else if(key == ' ')
		paused = !paused;
	else
		return false;

	return true;
}



bool ArenaPanel::Click(int x, int y, int clicks)
{
	// Don't respond to clicks if another panel is active.
	if(!canClick)
		return true;
	// Only allow drags that start when clicking was possible.
	canDrag = true;

	dragSource = Point(x, y);
	dragPoint = dragSource;

	SDL_Keymod mod = SDL_GetModState();
	hasShift = (mod & KMOD_SHIFT);

	engine.Click(dragSource, dragSource, hasShift);

	return true;
}



bool ArenaPanel::RClick(int x, int y)
{
	engine.RClick(Point(x, y));

	return true;
}



bool ArenaPanel::Drag(double dx, double dy)
{
	if(!canDrag)
		return true;

	center -= Point(dx, dy) / engine.zoom;
	engine.SetCustomCenter(center);
	isDragging = true;
	return true;
}



bool ArenaPanel::Release(int x, int y)
{
	if(isDragging)
	{
		dragPoint = Point(x, y);
		if(dragPoint.Distance(dragSource) > 5.)
			engine.Click(dragSource, dragPoint, hasShift);

		isDragging = false;
	}

	return true;
}



bool ArenaPanel::Scroll(double dx, double dy)
{
	if(dy < 0)
		Preferences::ZoomViewOut();
	else if(dy > 0)
		Preferences::ZoomViewIn();
	else
		return false;

	return true;
}



// Handle ShipEvents from this and previous Engine::Step calls. Start with the
// oldest and then process events until any create a new UI element.
void ArenaPanel::StepEvents(bool &isActive)
{
	while(isActive && !eventQueue.empty())
	{
		const ShipEvent &event = eventQueue.front();

		// Pass this event to the player, to update conditions and make
		// any new UI elements (e.g. an "on enter" dialog) from their
		// active missions.
		if(!handledFront)
			player.HandleEvent(event, GetUI());
		handledFront = true;
		isActive = (GetUI()->Top().get() == this);

		// If we can't safely display a new UI element (i.e. an active
		// mission created a UI element), then stop processing events
		// until the current Conversation or Dialog is resolved. This
		// will keep the current event in the queue, so we can still
		// check it for various special cases involving the player.
		if(!isActive)
			break;

		// Remove the fully-handled event.
		eventQueue.pop_front();
		handledFront = false;
	}
}
