// SPDX-License-Identifier: GPL-3.0

#include "OutfitterEditorPanel.h"

#include "text/alignment.hpp"
#include "Color.h"
#include "Dialog.h"
#include "text/DisplayText.h"
#include "Editor.h"
#include "FillShader.h"
#include "text/Font.h"
#include "text/FontSet.h"
#include "text/Format.h"
#include "Government.h"
#include "Hardpoint.h"
#include "text/layout.hpp"
#include "Logger.h"
#include "Outfit.h"
#include "OutlineShader.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Point.h"
#include "PointerShader.h"
#include "Preferences.h"
#include "Screen.h"
#include "Ship.h"
#include "ShipEditor.h"
#include "Sprite.h"
#include "SpriteSet.h"
#include "SpriteShader.h"
#include "text/truncate.hpp"
#include "UI.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string_view>

using namespace std;

namespace {
	string Tons(int tons)
	{
		return to_string(tons) + (tons == 1 ? " ton" : " tons");
	}

	const string SHIP_OUTLINES = "Ship outlines in shops";

	constexpr int ICON_TILE = 62;

	constexpr int SIDEBAR_WIDTH = 250;
	constexpr int INFOBAR_WIDTH = 300;
	constexpr int SIDE_WIDTH = SIDEBAR_WIDTH + INFOBAR_WIDTH;
	constexpr int BUTTON_HEIGHT = 70;
	constexpr int SHIP_SIZE = 250;
	constexpr int OUTFIT_SIZE = 183;

	constexpr string_view DEPRECATED_OUTFITS[] = {
		"Korath Tek'far Reactor",
		"Korath Tek'nel Reactor",
		"Korath Shield Generator",
		"Korath Heat Shunt",
		"Korath Ark'torbal Thruster",
		"Korath Ark'parat Steering",
		"Korath Jak'torbal Thruster",
		"Korath Jak'parat Steering",
		"Mass Expansion",
		"Reverse Thruster",
		"Bullfrog Anti-Missile Turret",
		"Chameleon Anti-Missile Turret",
		"Cruiser License",
		"Carrier License",
		"Afterburner",
		"Korath Grab-Strike",
		"Korath Banisher",
		"Korath Warder",
		"Korath Fire-Lance",
		"Korath Repeater",
		"Korath Repeater Turret",
		"Korath Piercer",
		"Korath Piercer Rack",
		"Korath Piercer Launcher",
		"Korath Mine",
		"Korath Mine Rack",
		"Korath Minelayer",
		"Korath Disruptor",
		"Korath Slicer",
		"Korath Slicer Turret",
		"Korath Repeater Rifle",
	};
}



void OutfitterEditorPanel::RenderProperties(bool &show)
{
	if(!ImGui::Begin("Outfitter Properties", &show, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::End();
		return;
	}

	ImGui::Checkbox("Show deprecated outfits", &showDeprecatedOutfits);
	ImGui::End();
}



OutfitterEditorPanel::OutfitterEditorPanel(Editor &editor, ShipEditor &shipEditor)
	: editor(editor), shipEditor(shipEditor), categories(editor.Universe().categories[CategoryType::OUTFIT])
{
	ship = shipEditor.GetShip();
	UpdateCache();
	SetIsFullScreen(true);
	SetInterruptible(false);
}



void OutfitterEditorPanel::Step()
{
	// Perform autoscroll to bring item details into view.
	if(scrollDetailsIntoView && mainDetailHeight > 0)
	{
		int mainTopY = Screen::Top();
		int mainBottomY = Screen::Bottom() - 40;
		double selectedBottomY = selectedTopY + TileSize() + mainDetailHeight;
		// Scroll up until the bottoms match.
		if(selectedBottomY > mainBottomY)
			DoScroll(max(-30., mainBottomY - selectedBottomY));
		// Scroll down until the bottoms or the tops match.
		else if(selectedBottomY < mainBottomY && (mainBottomY - mainTopY < selectedBottomY - selectedTopY && selectedTopY < mainTopY))
			DoScroll(min(30., min(mainTopY - selectedTopY, mainBottomY - selectedBottomY)));
		// Details are in view.
		else
			scrollDetailsIntoView = false;
	}
}



void OutfitterEditorPanel::Draw()
{
	const double oldSelectedTopY = selectedTopY;

	glClear(GL_COLOR_BUFFER_BIT);

	// Clear the list of clickable zones.
	zones.clear();
	categoryZones.clear();

	DrawShipsSidebar();
	DrawDetailsSidebar();
	DrawButtons();
	DrawMain();

	shipInfo.DrawTooltips();
	outfitInfo.DrawTooltips();

	if(!warningType.empty())
	{
		constexpr int WIDTH = 250;
		constexpr int PAD = 10;
		auto it = editor.Universe().tooltips.find(warningType);
		string text = (it == editor.Universe().tooltips.end() ? "" : it->second);
		WrappedText wrap(FontSet::Get(14));
		wrap.SetWrapWidth(WIDTH - 2 * PAD);
		wrap.Wrap(text);

		bool isError = (warningType.back() == '!');
		const Color &textColor = *editor.Universe().colors.Get("medium");
		const Color &backColor = *editor.Universe().colors.Get(isError ? "error back" : "warning back");

		Point size(WIDTH, wrap.Height() + 2 * PAD);
		Point anchor = Point(warningPoint.X(), min<double>(warningPoint.Y() + size.Y(), Screen::Bottom()));
		FillShader::Fill(anchor - .5 * size, size, backColor);
		wrap.Draw(anchor - size + Point(PAD, PAD), textColor);
	}

	if(sameSelectedTopY)
	{
		sameSelectedTopY = false;
		if(selectedTopY != oldSelectedTopY)
		{
			// Redraw with the same selected top (item in the same place).
			mainScroll = max(0., min(maxMainScroll, mainScroll + selectedTopY - oldSelectedTopY));
			Draw();
		}
	}
	mainScroll = min(mainScroll, maxMainScroll);
}



void OutfitterEditorPanel::UpdateCache()
{
	catalog.clear();
	for(const auto &it : editor.Universe().outfits)
		catalog[it.second.Category()].insert(it.first);
}



void OutfitterEditorPanel::DrawShipsSidebar()
{
	sideDetailHeight = 0;

	// Fill in the background.
	FillShader::Fill(
		Point(Screen::Right() - SIDEBAR_WIDTH / 2, 0.),
		Point(SIDEBAR_WIDTH, Screen::Height()),
		*editor.Universe().colors.Get("panel background"));
	FillShader::Fill(
		Point(Screen::Right() - SIDEBAR_WIDTH, 0.),
		Point(1, Screen::Height()),
		*editor.Universe().colors.Get("shop side panel background"));

	if(!ship)
		return;

	Point point(
		Screen::Right() - SIDEBAR_WIDTH / 2 - 93,
		Screen::Top() + SIDEBAR_WIDTH / 2 - sidebarScroll + 40 - 73);

	point.Y() += ICON_TILE;
	point.X() = Screen::Right() - SIDEBAR_WIDTH / 2;
	DrawShip(*ship, point, true);

	Point offset(SIDEBAR_WIDTH / -2, SHIP_SIZE / 2);

	// Check whether flight check tooltips should be shown.
	const auto flightChecks = FlightCheck();
	Point mouse = GetUI()->GetMouse();
	warningType.clear();

	static const Color selected(.8f, 1.f);
	static const Color unselected(.4f, 1.f);
	const auto checkIt = flightChecks.find(ship);
	if(checkIt != flightChecks.end())
	{
		const string &check = (*checkIt).second.front();
		const Sprite *icon = editor.Sprites().Get(check.back() == '!' ? "ui/error" : "ui/warning");
		SpriteShader::Draw(icon, point + offset + .5 * Point(ICON_TILE - icon->Width(), ICON_TILE - icon->Height() - 75.));

		zones.emplace_back(point + offset, Point(ICON_TILE, ICON_TILE), nullptr);
		if(zones.back().Contains(mouse))
		{
			warningType = check;
			warningPoint = zones.back().TopLeft();
		}
	}

	sideDetailHeight = DrawPlayerShipInfo(point + offset);
	point.Y() += sideDetailHeight + SHIP_SIZE / 2;
	maxSidebarScroll = max(0., point.Y() + sidebarScroll - Screen::Bottom() + BUTTON_HEIGHT);

	PointerShader::Draw(Point(Screen::Right() - 10, Screen::Top() + 10),
		Point(0., -1.), 10.f, 10.f, 5.f, Color(sidebarScroll > 0 ? .8f : .2f, 0.f));
	PointerShader::Draw(Point(Screen::Right() - 10, Screen::Bottom() - 80),
		Point(0., 1.), 10.f, 10.f, 5.f, Color(sidebarScroll < maxSidebarScroll ? .8f : .2f, 0.f));
}



void OutfitterEditorPanel::DrawDetailsSidebar()
{
	// Fill in the background.
	const Color &line = *editor.Universe().colors.Get("dim");
	const Color &back = *editor.Universe().colors.Get("shop info panel background");
	FillShader::Fill(
		Point(Screen::Right() - SIDEBAR_WIDTH - INFOBAR_WIDTH, 0.),
		Point(1., Screen::Height()),
		line);
	FillShader::Fill(
		Point(Screen::Right() - SIDEBAR_WIDTH - INFOBAR_WIDTH / 2, 0.),
		Point(INFOBAR_WIDTH - 1., Screen::Height()),
		back);

	Point point(
		Screen::Right() - SIDE_WIDTH + INFOBAR_WIDTH / 2,
		Screen::Top() + 30 - infobarScroll);

	int heightOffset = DrawDetails(point);

	maxInfobarScroll = max(0., heightOffset + infobarScroll - Screen::Bottom());

	PointerShader::Draw(Point(Screen::Right() - SIDEBAR_WIDTH - 10, Screen::Top() + 10),
		Point(0., -1.), 10.f, 10.f, 5.f, Color(infobarScroll > 0 ? .8f : .2f, 0.f));
	PointerShader::Draw(Point(Screen::Right() - SIDEBAR_WIDTH - 10, Screen::Bottom() - 10),
		Point(0., 1.), 10.f, 10.f, 5.f, Color(infobarScroll < maxInfobarScroll ? .8f : .2f, 0.f));
}



void OutfitterEditorPanel::DrawButtons()
{
	// The last 70 pixels on the end of the side panel are for the buttons:
	Point buttonSize(SIDEBAR_WIDTH, BUTTON_HEIGHT);
	FillShader::Fill(Screen::BottomRight() - .5 * buttonSize, buttonSize, *editor.Universe().colors.Get("shop side panel background"));
	FillShader::Fill(
		Point(Screen::Right() - SIDEBAR_WIDTH / 2, Screen::Bottom() - BUTTON_HEIGHT),
		Point(SIDEBAR_WIDTH, 1), *editor.Universe().colors.Get("shop side panel footer"));

	const Font &font = FontSet::Get(14);
	const Color &dim = *editor.Universe().colors.Get("medium");
	const Color &back = *editor.Universe().colors.Get("panel background");

	const Font &bigFont = FontSet::Get(18);
	const Color &hover = *editor.Universe().colors.Get("hover");
	const Color &active = *editor.Universe().colors.Get("active");
	const Color &inactive = *editor.Universe().colors.Get("inactive");

	const Point buyCenter = Screen::BottomRight() - Point(210, 25);
	FillShader::Fill(buyCenter, Point(60, 30), back);
	string BUY = "_Buy";
	bigFont.Draw(BUY,
		buyCenter - .5 * Point(bigFont.Width(BUY), bigFont.Height()),
		CanBuy() ? hoverButton == 'b' ? hover : active : inactive);

	const Point sellCenter = Screen::BottomRight() - Point(130, 25);
	FillShader::Fill(sellCenter, Point(60, 30), back);
	static const string SELL = "_Sell";
	bigFont.Draw(SELL,
		sellCenter - .5 * Point(bigFont.Width(SELL), bigFont.Height()),
		CanSell() ? hoverButton == 's' ? hover : active : inactive);

	const Point leaveCenter = Screen::BottomRight() - Point(45, 25);
	FillShader::Fill(leaveCenter, Point(70, 30), back);
	static const string LEAVE = "_Leave";
	bigFont.Draw(LEAVE,
		leaveCenter - .5 * Point(bigFont.Width(LEAVE), bigFont.Height()),
		hoverButton == 'l' ? hover : active);

	int modifier = Modifier();
	if(modifier > 1)
	{
		string mod = "x " + to_string(modifier);
		int modWidth = font.Width(mod);
		font.Draw(mod, buyCenter + Point(-.5 * modWidth, 10.), dim);
		font.Draw(mod, sellCenter + Point(-.5 * modWidth, 10.), dim);
	}
}



void OutfitterEditorPanel::DrawMain()
{
	const Font &bigFont = FontSet::Get(18);
	const Color &dim = *editor.Universe().colors.Get("medium");
	const Color &bright = *editor.Universe().colors.Get("bright");
	mainDetailHeight = 0;

	const Sprite *collapsedArrow = editor.Sprites().Get("ui/collapsed");
	const Sprite *expandedArrow = editor.Sprites().Get("ui/expanded");

	// Draw all the available items.
	// First, figure out how many columns we can draw.
	const int TILE_SIZE = TileSize();
	const int mainWidth = (Screen::Width() - SIDE_WIDTH - 1);
	// If the user horizontally compresses the window too far, draw nothing.
	if(mainWidth < TILE_SIZE)
		return;
	const int columns = mainWidth / TILE_SIZE;
	const int columnWidth = mainWidth / columns;

	const Point begin(
		(Screen::Width() - columnWidth) / -2,
		(Screen::Height() - TILE_SIZE) / -2 - mainScroll + 20);
	Point point = begin;
	const float endX = Screen::Right() - (SIDE_WIDTH + 1);
	double nextY = begin.Y() + TILE_SIZE;
	int scrollY = 0;
	for(const auto &cat : categories)
	{
		const auto &category = cat.Name();
		auto it = catalog.find(category);
		if(it == catalog.end())
			continue;

		Point side(Screen::Left() + 5., point.Y() - TILE_SIZE / 2 + 10);
		point.Y() += bigFont.Height() + 20;
		nextY += bigFont.Height() + 20;

		bool isCollapsed = collapsed.count(category);
		bool isEmpty = true;
		for(const string &name : it->second)
		{
			if(!showDeprecatedOutfits && find(std::begin(DEPRECATED_OUTFITS), end(DEPRECATED_OUTFITS), name) != end(DEPRECATED_OUTFITS))
				continue;
			const bool isSelected = (selectedOutfit && editor.Universe().outfits.Get(name) == selectedOutfit);

			if(isSelected)
				selectedTopY = point.Y() - TILE_SIZE / 2;

			if(!HasItem(name))
				continue;
			isEmpty = false;
			if(isCollapsed)
				break;

			DrawItem(name, point, scrollY);

			point.X() += columnWidth;
			if(point.X() >= endX)
			{
				point.X() = begin.X();
				point.Y() = nextY;
				nextY += TILE_SIZE;
				scrollY = -mainDetailHeight;
			}
		}

		if(!isEmpty)
		{
			Point size(bigFont.Width(category) + 25., bigFont.Height());
			categoryZones.emplace_back(Point(Screen::Left(), side.Y()) + .5 * size, size, category);
			SpriteShader::Draw(isCollapsed ? collapsedArrow : expandedArrow, side + Point(10., 10.));
			bigFont.Draw(category, side + Point(25., 0.), isCollapsed ? dim : bright);

			if(point.X() != begin.X())
			{
				point.X() = begin.X();
				point.Y() = nextY;
				nextY += TILE_SIZE;
				scrollY = -mainDetailHeight;
			}
			point.Y() += 40;
			nextY += 40;
		}
		else
		{
			point.Y() -= bigFont.Height() + 20;
			nextY -= bigFont.Height() + 20;
		}
	}
	// This is how much Y space was actually used.
	nextY -= 40 + TILE_SIZE;

	// What amount would mainScroll have to equal to make nextY equal the
	// bottom of the screen? (Also leave space for the "key" at the bottom.)
	maxMainScroll = max(0., nextY + mainScroll - Screen::Height() / 2 - TILE_SIZE / 2 + VisiblityCheckboxesSize());

	PointerShader::Draw(Point(Screen::Right() - 10 - SIDE_WIDTH, Screen::Top() + 10),
		Point(0., -1.), 10.f, 10.f, 5.f, Color(mainScroll > 0 ? .8f : .2f, 0.f));
	PointerShader::Draw(Point(Screen::Right() - 10 - SIDE_WIDTH, Screen::Bottom() - 10),
		Point(0., 1.), 10.f, 10.f, 5.f, Color(mainScroll < maxMainScroll ? .8f : .2f, 0.f));
}



void OutfitterEditorPanel::DrawShip(const Ship &ship, const Point &center, bool isSelected)
{
	const Sprite *back = editor.Sprites().Get(
		isSelected ? "ui/shipyard selected" : "ui/shipyard unselected");
	SpriteShader::Draw(back, center);

	const Sprite *thumbnail = ship.Thumbnail();
	const Sprite *sprite = ship.GetSprite();
	int swizzle = ship.CustomSwizzle() >= 0 ? ship.CustomSwizzle() : 0;
	if(thumbnail)
		SpriteShader::Draw(thumbnail, center + Point(0., 10.), 1., swizzle);
	else if(sprite)
	{
		// Make sure the ship sprite leaves 10 pixels padding all around.
		const float zoomSize = SHIP_SIZE - 60.f;
		float zoom = min(1.f, zoomSize / max(sprite->Width(), sprite->Height()));
		SpriteShader::Draw(sprite, center, zoom, swizzle);
	}

	// Draw the ship name.
	const Font &font = FontSet::Get(14);
	const string &name = ship.Name().empty() ? ship.ModelName() : ship.Name();
	Point offset(-SIDEBAR_WIDTH / 2, -.5f * SHIP_SIZE + 10.f);
	font.Draw({name, {SIDEBAR_WIDTH, Alignment::CENTER, Truncate::MIDDLE}},
		center + offset, *editor.Universe().colors.Get("bright"));
}



int OutfitterEditorPanel::TileSize() const
{
	return OUTFIT_SIZE;
}



int OutfitterEditorPanel::VisiblityCheckboxesSize() const
{
	return 60;
}



int OutfitterEditorPanel::DrawPlayerShipInfo(const Point &point)
{
	shipInfo.Update(*ship, PlayerInfo(), 0);
	shipInfo.DrawAttributes(point);

	return shipInfo.AttributesHeight();
}



bool OutfitterEditorPanel::HasItem(const string &name) const
{
	return true;
}



void OutfitterEditorPanel::DrawItem(const string &name, const Point &point, int scrollY)
{
	const Outfit *outfit = editor.Universe().outfits.Get(name);
	zones.emplace_back(point, Point(OUTFIT_SIZE, OUTFIT_SIZE), outfit, scrollY);
	if(point.Y() + OUTFIT_SIZE / 2 < Screen::Top() || point.Y() - OUTFIT_SIZE / 2 > Screen::Bottom())
		return;

	bool isSelected = (outfit == selectedOutfit);
	bool isOwned = ship && ship->OutfitCount(outfit);
	DrawOutfit(*outfit, point, isSelected, isOwned);

	// Check if this outfit is a "license".
	bool isLicense = IsLicense(name);
	int mapSize = outfit->Get("map");

	const Font &font = FontSet::Get(14);
	const Color &bright = *editor.Universe().colors.Get("bright");
	if(ship || isLicense || mapSize)
	{
		int minCount = numeric_limits<int>::max();
		int maxCount = 0;
		if(isLicense)
			minCount = maxCount = 0;
		else if(mapSize)
			minCount = maxCount = 0;
		else
		{
			int count = ship->OutfitCount(outfit);
			minCount = min(minCount, count);
			maxCount = max(maxCount, count);
		}

		if(maxCount)
		{
			string label = "installed: " + to_string(minCount);
			if(maxCount > minCount)
				label += " - " + to_string(maxCount);

			Point labelPos = point + Point(-OUTFIT_SIZE / 2 + 20, OUTFIT_SIZE / 2 - 38);
			font.Draw(label, labelPos, bright);
		}
	}
}



int OutfitterEditorPanel::DividerOffset() const
{
	return 80;
}



int OutfitterEditorPanel::DetailWidth() const
{
	return 3 * outfitInfo.PanelWidth();
}



int OutfitterEditorPanel::DrawDetails(const Point &center)
{
	string selectedItem = "Nothing Selected";
	const Font &font = FontSet::Get(14);
	const Color &bright = *editor.Universe().colors.Get("bright");
	const Color &dim = *editor.Universe().colors.Get("medium");
	const Sprite *collapsedArrow = editor.Sprites().Get("ui/collapsed");

	int heightOffset = 20;

	if(selectedOutfit)
	{
		outfitInfo.Update(*selectedOutfit, PlayerInfo(), CanSell());
		selectedItem = selectedOutfit->TrueName();

		const Sprite *thumbnail = selectedOutfit->Thumbnail();
		const Sprite *background = editor.Sprites().Get("ui/outfitter selected");

		float tileSize = thumbnail
			? max(thumbnail->Height(), static_cast<float>(TileSize()))
			: static_cast<float>(TileSize());

		Point thumbnailCenter(center.X(), center.Y() + 20 + tileSize / 2);

		Point startPoint(center.X() - INFOBAR_WIDTH / 2 + 20, center.Y() + 20 + tileSize);

		double descriptionOffset = 35.;
		Point descCenter(Screen::Right() - SIDE_WIDTH + INFOBAR_WIDTH / 2, startPoint.Y() + 20.);

		// Maintenance note: This can be replaced with collapsed.contains() in C++20
		if(!collapsed.count("description"))
		{
			descriptionOffset = outfitInfo.DescriptionHeight();
			outfitInfo.DrawDescription(startPoint);
		}
		else
		{
			std::string label = "description";
			font.Draw(label, startPoint + Point(35., 12.), dim);
			SpriteShader::Draw(collapsedArrow, startPoint + Point(20., 20.));
		}

		// Calculate the new ClickZone for the description.
		Point descDimensions(INFOBAR_WIDTH, descriptionOffset + 10.);
		ClickZone<std::string> collapseDescription = ClickZone<std::string>(descCenter, descDimensions, std::string("description"));

		// Find the old zone, and replace it with the new zone.
		for(auto it = categoryZones.begin(); it != categoryZones.end(); ++it)
		{
			if(it->Value() == "description")
			{
				categoryZones.erase(it);
				break;
			}
		}
		categoryZones.emplace_back(collapseDescription);

		Point attrPoint(startPoint.X(), startPoint.Y() + descriptionOffset);
		Point reqsPoint(startPoint.X(), attrPoint.Y() + outfitInfo.AttributesHeight());

		SpriteShader::Draw(background, thumbnailCenter);
		if(thumbnail)
			SpriteShader::Draw(thumbnail, thumbnailCenter);

		outfitInfo.DrawAttributes(attrPoint);
		outfitInfo.DrawRequirements(reqsPoint);

		heightOffset = reqsPoint.Y() + outfitInfo.RequirementsHeight();
	}

	// Draw this string representing the selected item (if any), centered in the details side panel
	Point selectedPoint(center.X() - .5 * INFOBAR_WIDTH, center.Y());
	font.Draw({selectedItem, {INFOBAR_WIDTH - 20, Alignment::CENTER, Truncate::MIDDLE}},
		selectedPoint, bright);

	return heightOffset;
}



bool OutfitterEditorPanel::CanBuy(bool checkAlreadyOwned) const
{
	if(!ship || !selectedOutfit)
		return false;

	if(ShipCanBuy(ship, selectedOutfit))
		return true;

	return false;
}



void OutfitterEditorPanel::Buy(bool alreadyOwned)
{
	int modifier = Modifier();
	for(int i = 0; i < modifier && CanBuy(alreadyOwned); ++i)
	{
		// Special case: maps.
		int mapSize = selectedOutfit->Get("map");
		if(mapSize > 0)
			return;

		// Special case: licenses.
		if(IsLicense(selectedOutfit->TrueName()))
			return;

		if(!CanBuy(alreadyOwned))
			return;

		ship->AddOutfit(selectedOutfit, 1);
		int required = selectedOutfit->Get("required crew");
		if(required && ship->Crew() + required <= static_cast<int>(ship->Attributes().Get("bunks")))
			ship->AddCrew(required);
		ship->Recharge();
		shipEditor.SetModified();
	}
}



void OutfitterEditorPanel::FailBuy() const
{
	if(!selectedOutfit)
		return;

	if(selectedOutfit->Get("map"))
	{
		GetUI()->Push(new Dialog("You have already mapped all the systems shown by this map, "
			"so there is no reason to buy another."));
		return;
	}

	if(HasLicense(selectedOutfit->TrueName()))
	{
		GetUI()->Push(new Dialog("You already have one of these licenses, "
			"so there is no reason to buy another."));
		return;
	}

	double outfitNeeded = -selectedOutfit->Get("outfit space");
	double outfitSpace = ship->Attributes().Get("outfit space");
	if(outfitNeeded > outfitSpace)
	{
		GetUI()->Push(new Dialog("You cannot install this outfit, because it takes up "
			+ Tons(outfitNeeded) + " of outfit space, and this ship has "
			+ Tons(outfitSpace) + " free."));
		return;
	}

	double weaponNeeded = -selectedOutfit->Get("weapon capacity");
	double weaponSpace = ship->Attributes().Get("weapon capacity");
	if(weaponNeeded > weaponSpace)
	{
		GetUI()->Push(new Dialog("Only part of your ship's outfit capacity is usable for weapons. "
			"You cannot install this outfit, because it takes up "
			+ Tons(weaponNeeded) + " of weapon space, and this ship has "
			+ Tons(weaponSpace) + " free."));
		return;
	}

	double engineNeeded = -selectedOutfit->Get("engine capacity");
	double engineSpace = ship->Attributes().Get("engine capacity");
	if(engineNeeded > engineSpace)
	{
		GetUI()->Push(new Dialog("Only part of your ship's outfit capacity is usable for engines. "
			"You cannot install this outfit, because it takes up "
			+ Tons(engineNeeded) + " of engine space, and this ship has "
			+ Tons(engineSpace) + " free."));
		return;
	}

	if(selectedOutfit->Category() == "Ammunition")
	{
		if(!ship->OutfitCount(selectedOutfit))
			GetUI()->Push(new Dialog("This outfit is ammunition for a weapon. "
				"You cannot install it without first installing the appropriate weapon."));
		else
			GetUI()->Push(new Dialog("You already have the maximum amount of ammunition for this weapon. "
				"If you want to install more ammunition, you must first install another of these weapons."));
		return;
	}

	int mountsNeeded = -selectedOutfit->Get("turret mounts");
	int mountsFree = ship->Attributes().Get("turret mounts");
	if(mountsNeeded && !mountsFree)
	{
		GetUI()->Push(new Dialog("This weapon is designed to be installed on a turret mount, "
			"but your ship does not have any unused turret mounts available."));
		return;
	}

	int gunsNeeded = -selectedOutfit->Get("gun ports");
	int gunsFree = ship->Attributes().Get("gun ports");
	if(gunsNeeded && !gunsFree)
	{
		GetUI()->Push(new Dialog("This weapon is designed to be installed in a gun port, "
			"but your ship does not have any unused gun ports available."));
		return;
	}

	if(selectedOutfit->Get("installable") < 0.)
	{
		GetUI()->Push(new Dialog("This item is not an outfit that can be installed in a ship."));
		return;
	}

	if(!ship->Attributes().CanAdd(*selectedOutfit, 1))
	{
		GetUI()->Push(new Dialog("You cannot install this outfit in your ship, "
			"because it would reduce one of your ship's attributes to a negative amount. "
			"For example, it may use up more cargo space than you have left."));
		return;
	}
}



bool OutfitterEditorPanel::CanSell(bool toStorage) const
{
	if(!selectedOutfit)
		return false;

	if(ShipCanSell(ship, selectedOutfit))
		return true;

	return false;
}



void OutfitterEditorPanel::Sell(bool toStorage)
{
	ship->AddOutfit(selectedOutfit, -1);
	if(selectedOutfit->Get("required crew"))
		ship->AddCrew(-selectedOutfit->Get("required crew"));
	ship->Recharge();
	shipEditor.SetModified();

	const Outfit *ammo = selectedOutfit->Ammo();
	if(ammo && ship->OutfitCount(ammo))
	{
		// Determine how many of this ammo I must sell to also sell the launcher.
		int mustSell = 0;
		for(const pair<const char *, double> &it : ship->Attributes().Attributes())
			if(it.second < 0.)
				mustSell = max<int>(mustSell, it.second / ammo->Get(it.first));

		if(mustSell)
			ship->AddOutfit(ammo, -mustSell);
	}
}



void OutfitterEditorPanel::FailSell(bool toStorage) const
{
	const string &verb = toStorage ? "uninstall" : "sell";
	if(!selectedOutfit)
		return;
	else if(selectedOutfit->Get("map"))
		GetUI()->Push(new Dialog("You cannot " + verb + " maps. Once you buy one, it is yours permanently."));
	else if(HasLicense(selectedOutfit->TrueName()))
		GetUI()->Push(new Dialog("You cannot " + verb + " licenses. Once you obtain one, it is yours permanently."));
	else
	{
		if(!ship->OutfitCount(selectedOutfit))
			GetUI()->Push(new Dialog("You do not have any of these outfits to " + verb + "."));
		else
		{
			for(const pair<const char *, double> &it : selectedOutfit->Attributes())
				if(ship->Attributes().Get(it.first) < it.second)
				{
					for(const auto &sit : ship->Outfits())
						if(sit.first->Get(it.first) < 0.)
						{
							GetUI()->Push(new Dialog("You cannot " + verb + " this outfit, "
								"because that would cause your ship's \"" + it.first +
								"\" value to be reduced to less than zero. "
								"To " + verb + " this outfit, you must " + verb + " the " +
								sit.first->TrueName() + " outfit first."));
							return;
						}
					GetUI()->Push(new Dialog("You cannot " + verb + " this outfit, "
						"because that would cause your ship's \"" + it.first +
						"\" value to be reduced to less than zero."));
					return;
				}
			GetUI()->Push(new Dialog("You cannot " + verb + " this outfit, "
				"because something else in your ship depends on it."));
		}
	}
}



bool OutfitterEditorPanel::ShouldHighlight(const Ship *ship)
{
	if(!selectedOutfit)
		return false;

	if(hoverButton == 'b')
		return CanBuy() && ShipCanBuy(ship, selectedOutfit);
	else if(hoverButton == 's')
		return CanSell() && ShipCanSell(ship, selectedOutfit);

	return false;
}



bool OutfitterEditorPanel::ShipCanBuy(const Ship *ship, const Outfit *outfit) const
{
	return (ship->Attributes().CanAdd(*outfit, 1) > 0);
}



bool OutfitterEditorPanel::ShipCanSell(const Ship *ship, const Outfit *outfit) const
{
	if(!ship || !ship->OutfitCount(outfit))
		return false;

	// If this outfit requires ammo, check if we could sell it if we sold all
	// the ammo for it first.
	const Outfit *ammo = outfit->Ammo();
	if(ammo && ship->OutfitCount(ammo))
	{
		Outfit attributes = ship->Attributes();
		attributes.Add(*ammo, -ship->OutfitCount(ammo));
		return attributes.CanAdd(*outfit, -1);
	}

	// Now, check whether this ship can sell this outfit.
	return ship->Attributes().CanAdd(*outfit, -1);
}



void OutfitterEditorPanel::DrawOutfit(const Outfit &outfit, const Point &center, bool isSelected, bool isOwned) const
{
	const Sprite *thumbnail = outfit.Thumbnail();
	const Sprite *back = editor.Sprites().Get(
		isSelected ? "ui/outfitter selected" : "ui/outfitter unselected");
	SpriteShader::Draw(back, center);
	SpriteShader::Draw(thumbnail, center);

	// Draw the outfit name.
	const string &name = outfit.TrueName();
	const Font &font = FontSet::Get(14);
	Point offset(-.5 * OUTFIT_SIZE, -.5 * OUTFIT_SIZE + 10.);
	font.Draw({name, {OUTFIT_SIZE, Alignment::CENTER, Truncate::MIDDLE}},
		center + offset, Color((isSelected | isOwned) ? .8 : .5, 0.));
}



bool OutfitterEditorPanel::IsLicense(const string &name) const
{
	static const string &LICENSE = " License";
	if(name.length() < LICENSE.length())
		return false;
	if(name.compare(name.length() - LICENSE.length(), LICENSE.length(), LICENSE))
		return false;

	return true;
}



bool OutfitterEditorPanel::HasLicense(const string &name) const
{
	return IsLicense(name);
}



string OutfitterEditorPanel::LicenseName(const string &name) const
{
	static const string &LICENSE = " License";
	return "license: " + name.substr(0, name.length() - LICENSE.length());
}



bool OutfitterEditorPanel::DoScroll(double dy)
{
	double *scroll = &mainScroll;
	double maximum = maxMainScroll;
	if(activePane == ShopPane::Info)
	{
		scroll = &infobarScroll;
		maximum = maxInfobarScroll;
	}
	else if(activePane == ShopPane::Sidebar)
	{
		scroll = &sidebarScroll;
		maximum = maxSidebarScroll;
	}

	*scroll = max(0., min(maximum, *scroll - dy));

	return true;
}



bool OutfitterEditorPanel::SetScrollToTop()
{
	if(activePane == ShopPane::Info)
		infobarScroll = 0.;
	else if(activePane == ShopPane::Sidebar)
		sidebarScroll = 0.;
	else
		mainScroll = 0.;

	return true;
}



bool OutfitterEditorPanel::SetScrollToBottom()
{
	if(activePane == ShopPane::Info)
		infobarScroll = maxInfobarScroll;
	else if(activePane == ShopPane::Sidebar)
		sidebarScroll = maxSidebarScroll;
	else
		mainScroll = maxMainScroll;

	return true;
}



void OutfitterEditorPanel::MainLeft()
{
	vector<Zone>::const_iterator start = MainStart();
	if(start == zones.end())
		return;

	vector<Zone>::const_iterator it = Selected();
	// Special case: nothing is selected. Go to the last item.
	if(it == zones.end())
	{
		--it;
		mainScroll = maxMainScroll;
		selectedOutfit = it->Value();
		return;
	}

	if(it == start)
	{
		mainScroll = 0;
		selectedOutfit = nullptr;
	}
	else
	{
		int previousY = it->Center().Y();
		--it;
		mainScroll += it->Center().Y() - previousY;
		if(mainScroll < 0)
			mainScroll = 0;
		selectedOutfit = it->Value();
	}
}



void OutfitterEditorPanel::MainRight()
{
	vector<Zone>::const_iterator start = MainStart();
	if(start == zones.end())
		return;

	vector<Zone>::const_iterator it = Selected();
	// Special case: nothing is selected. Select the first item.
	if(it == zones.end())
	{
		// Already at mainScroll = 0, no scrolling needed.
		selectedOutfit = start->Value();
		return;
	}

	int previousY = it->Center().Y();
	++it;
	if(it == zones.end())
	{
		mainScroll = 0;
		selectedOutfit = nullptr;
	}
	else
	{
		if(it->Center().Y() != previousY)
			mainScroll += it->Center().Y() - previousY - mainDetailHeight;
		if(mainScroll > maxMainScroll)
			mainScroll = maxMainScroll;
		selectedOutfit = it->Value();
	}
}



void OutfitterEditorPanel::MainUp()
{
	vector<Zone>::const_iterator start = MainStart();
	if(start == zones.end())
		return;

	vector<Zone>::const_iterator it = Selected();
	// Special case: nothing is selected. Go to the last item.
	if(it == zones.end())
	{
		--it;
		mainScroll = maxMainScroll;
		selectedOutfit = it->Value();
		return;
	}

	int previousX = it->Center().X();
	int previousY = it->Center().Y();
	while(it != start && it->Center().Y() == previousY)
		--it;
	while(it != start && it->Center().X() > previousX)
		--it;

	if(it == start && it->Center().Y() == previousY)
	{
		mainScroll = 0;
		selectedOutfit = nullptr;
	}
	else
	{
		mainScroll += it->Center().Y() - previousY;
		if(mainScroll < 0)
			mainScroll = 0;
		selectedOutfit = it->Value();
	}
}



void OutfitterEditorPanel::MainDown()
{
	vector<Zone>::const_iterator start = MainStart();
	if(start == zones.end())
		return;

	vector<Zone>::const_iterator it = Selected();
	// Special case: nothing is selected. Select the first item.
	if(it == zones.end())
	{
		// Already at mainScroll = 0, no scrolling needed.
		selectedOutfit = start->Value();
		return;
	}

	int previousX = it->Center().X();
	int previousY = it->Center().Y();
	while(it != zones.end() && it->Center().Y() == previousY)
		++it;
	if(it == zones.end())
	{
		mainScroll = 0;
		selectedOutfit = nullptr;
		return;
	}

	int newY = it->Center().Y();
	while(it != zones.end() && it->Center().X() <= previousX && it->Center().Y() == newY)
		++it;
	--it;

	mainScroll = min(mainScroll + it->Center().Y() - previousY - mainDetailHeight, maxMainScroll);
	selectedOutfit = it->Value();
}



vector<OutfitterEditorPanel::Zone>::const_iterator OutfitterEditorPanel::Selected() const
{
	// Find the object that was clicked on.
	vector<Zone>::const_iterator it = MainStart();
	for( ; it != zones.end(); ++it)
		if(it->Value() == selectedOutfit)
			break;

	return it;
}



vector<OutfitterEditorPanel::Zone>::const_iterator OutfitterEditorPanel::MainStart() const
{
	// Find the first non-player-ship click zone.
	int margin = Screen::Right() - SHIP_SIZE;
	vector<Zone>::const_iterator start = zones.begin();
	while(start != zones.end() && start->Center().X() > margin)
		++start;

	return start;
}



bool OutfitterEditorPanel::Click(int x, int y, int /* clicks */)
{
	// Handle clicks on the buttons.
	char button = CheckButton(x, y);
	if(button)
		return DoKey(button);

	// Check for clicks in the scroll arrows.
	if(x >= Screen::Right() - 20)
	{
		if(y < Screen::Top() + 20)
			return Scroll(0, 4);
		if(y < Screen::Bottom() - BUTTON_HEIGHT && y >= Screen::Bottom() - BUTTON_HEIGHT - 20)
			return Scroll(0, -4);
	}
	else if(x >= Screen::Right() - SIDE_WIDTH - 20 && x < Screen::Right() - SIDE_WIDTH)
	{
		if(y < Screen::Top() + 20)
			return Scroll(0, 4);
		if(y >= Screen::Bottom() - 20)
			return Scroll(0, -4);
	}

	const Point clickPoint(x, y);

	// Check for clicks in the category labels.
	for(const ClickZone<string> &zone : categoryZones)
		if(zone.Contains(clickPoint))
		{
			bool toggleAll = (SDL_GetModState() & KMOD_SHIFT);
			auto it = collapsed.find(zone.Value());
			if(it == collapsed.end())
			{
				if(toggleAll)
				{
					selectedOutfit = nullptr;
					for(const auto &category : categories)
						collapsed.insert(category.Name());
				}
				else
				{
					collapsed.insert(zone.Value());
					if(selectedOutfit && selectedOutfit->Category() == zone.Value())
						selectedOutfit = nullptr;
				}
			}
			else
			{
				if(toggleAll)
					collapsed.clear();
				else
					collapsed.erase(it);
			}
			return true;
		}

	// Handle clicks anywhere else by checking if they fell into any of the
	// active click zones (main panel or side panel).
	for(const Zone &zone : zones)
		if(zone.Contains(clickPoint))
		{
			selectedOutfit = zone.Value();

			// Scroll details into view in Step() when the height is known.
			scrollDetailsIntoView = true;
			mainDetailHeight = 0;
			mainScroll = max(0., mainScroll + zone.ScrollY());
			return true;
		}

	return true;
}



bool OutfitterEditorPanel::Hover(int x, int y)
{
	Point point(x, y);
	// Check that the point is not in the button area.
	hoverButton = CheckButton(x, y);
	if(hoverButton)
	{
		shipInfo.ClearHover();
		outfitInfo.ClearHover();
	}
	else
	{
		shipInfo.Hover(point);
		outfitInfo.Hover(point);
	}

	activePane = ShopPane::Main;
	if(x > Screen::Right() - SIDEBAR_WIDTH)
		activePane = ShopPane::Sidebar;
	else if(x > Screen::Right() - SIDE_WIDTH)
		activePane = ShopPane::Info;
	return true;
}



bool OutfitterEditorPanel::Scroll(double dx, double dy)
{
	scrollDetailsIntoView = false;
	return DoScroll(dy * 2.5 * Preferences::ScrollSpeed());
}



// Check if the given point is within the button zone, and if so return the
// letter of the button (or ' ' if it's not on a button).
char OutfitterEditorPanel::CheckButton(int x, int y)
{
	if(x < Screen::Right() - SIDEBAR_WIDTH || y < Screen::Bottom() - BUTTON_HEIGHT)
		return '\0';

	if(y < Screen::Bottom() - 40 || y >= Screen::Bottom() - 10)
		return ' ';

	x -= Screen::Right() - SIDEBAR_WIDTH;
	if(x > 9 && x < 70)
		return 'b';
	else if(x > 89 && x < 150)
		return 's';
	else if(x > 169 && x < 240)
		return 'l';

	return ' ';
}



OutfitterEditorPanel::Zone::Zone(Point center, Point size, const Outfit *outfit, double scrollY)
	: ClickZone(center, size, outfit), scrollY(scrollY)
{
}



double OutfitterEditorPanel::Zone::ScrollY() const
{
	return scrollY;
}



// Only override the ones you need; the default action is to return false.
bool OutfitterEditorPanel::KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress)
{
	scrollDetailsIntoView = false;
	if(key == 'l' || key == 'd' || key == SDLK_ESCAPE
			|| (key == 'w' && (mod & (KMOD_CTRL | KMOD_GUI))))
	{
		GetUI()->Pop(this);
	}
	else if(key == 'b' || ((key == 'i' || key == 'c') && selectedOutfit))
	{
		if(!CanBuy(key == 'i' || key == 'c'))
			FailBuy();
		else
			Buy(key == 'i' || key == 'c');
	}
	else if(key == 's' && !(mod & KMOD_CTRL))
	{
		if(!CanSell(false))
			FailSell(false);
		else
		{
			int modifier = Modifier();
			for(int i = 0; i < modifier && CanSell(false); ++i)
				Sell(false);
		}
	}
	else if(key == SDLK_LEFT)
	{
		if(activePane != ShopPane::Sidebar)
			MainLeft();
		else
			return false;
		return true;
	}
	else if(key == SDLK_RIGHT)
	{
		if(activePane != ShopPane::Sidebar)
			MainRight();
		else
			return false;
		return true;
	}
	else if(key == SDLK_UP)
	{
		if(activePane != ShopPane::Sidebar)
			MainUp();
		else
			return false;
		return true;
	}
	else if(key == SDLK_DOWN)
	{
		if(activePane != ShopPane::Sidebar)
			MainDown();
		else
			return false;
		return true;
	}
	else if(key == SDLK_PAGEUP)
		return DoScroll(Screen::Bottom());
	else if(key == SDLK_PAGEDOWN)
		return DoScroll(Screen::Top());
	else if(key == SDLK_HOME)
		return SetScrollToTop();
	else if(key == SDLK_END)
		return SetScrollToBottom();
	else if(key == SDLK_TAB)
		activePane = (activePane == ShopPane::Main ? ShopPane::Sidebar : ShopPane::Main);
	else
		return false;

	return true;
}



map<const Ship *, vector<string>> OutfitterEditorPanel::FlightCheck() const
{
	// Count of all bay types in the active fleet.
	auto bayCount = map<string, size_t>{};
	// Classification of the present ships by category. Parked ships are ignored.
	auto categoryCount = map<string, vector<const Ship *>>{};

	auto flightChecks = map<const Ship *, vector<string>>{};
	auto checks = ship->FlightCheck();
	if(!checks.empty())
		flightChecks.emplace(ship, checks);

	categoryCount[ship->Attributes().Category()].emplace_back(ship);
	if(!ship->CanBeCarried() && ship->HasBays())
	{
		for(auto &bay : ship->Bays())
		{
			++bayCount[bay.category];
			// The bays should always be empty. But if not, count that ship too.
			if(bay.ship)
			{
				Logger::LogError("Expected bay to be empty for " + ship->ModelName() + ": " + ship->Name());
				categoryCount[bay.ship->Attributes().Category()].emplace_back(bay.ship.get());
			}
		}
	}

	// Identify transportable ships that cannot jump and have no bay to be carried in.
	for(auto &bayType : bayCount)
	{
		const auto &shipsOfType = categoryCount[bayType.first];
		if(shipsOfType.empty())
			continue;
		for(const auto &carriable : shipsOfType)
		{
			if(carriable->JumpsRemaining() != 0)
			{
				// This ship can travel between systems and does not require a bay.
			}
			// This ship requires a bay to travel between systems.
			else if(bayType.second > 0)
				--bayType.second;
			else
			{
				// Include the lack of bay availability amongst any other
				// warnings for this carriable ship.
				auto it = flightChecks.find(carriable);
				string warning = "no bays?";
				if(it != flightChecks.end())
					it->second.emplace_back(warning);
				else
					flightChecks.emplace(carriable, vector<string>{warning});
			}
		}
	}
	return flightChecks;
}
