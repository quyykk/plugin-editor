// SPDX-License-Identifier: GPL-3.0

#ifndef OUTFITTER_EDITOR_PANEL_H_
#define OUTFITTER_EDITOR_PANEL_H_

#include "Panel.h"

#include "ClickZone.h"
#include "OutfitInfoDisplay.h"
#include "ShipInfoDisplay.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class Editor;
class Outfit;
class PlayerInfo;
class Point;
class Ship;
class ShipEditor;



class OutfitterEditorPanel : public Panel {
public:
	static void RenderProperties(bool &show);

	static inline bool showDeprecatedOutfits = false;


public:
	OutfitterEditorPanel(Editor &editor, ShipEditor &shipEditor);

	virtual void Step() override;
	virtual void Draw() override;

	void SetShip(Ship *ship) { this->ship = ship; }
	void UpdateCache();


protected:
	void DrawShipsSidebar();
	void DrawDetailsSidebar();
	void DrawButtons();
	void DrawMain();

	void DrawShip(const Ship &ship, const Point &center, bool isSelected);
	int TileSize() const;
	int VisiblityCheckboxesSize() const;
	int DrawPlayerShipInfo(const Point &point);
	bool HasItem(const std::string &name) const;
	void DrawItem(const std::string &name, const Point &point, int scrollY);
	int DividerOffset() const;
	int DetailWidth() const;
	int DrawDetails(const Point &center);
	bool CanBuy(bool checkAlreadyOwned = true) const;
	void Buy(bool alreadyOwned = false);
	void FailBuy() const;
	bool CanSell(bool toStorage = false) const;
	void Sell(bool toStorage = false);
	void FailSell(bool toStorage = false) const;
	bool ShouldHighlight(const Ship *ship);

	// Only override the ones you need; the default action is to return false.
	virtual bool KeyDown(SDL_Keycode key, Uint16 mod, const Command &command, bool isNewPress) override;
	virtual bool Click(int x, int y, int clicks) override;
	virtual bool Hover(int x, int y) override;
	virtual bool Scroll(double dx, double dy) override;


private:
	class Zone : public ClickZone<const Outfit *> {
	public:
		explicit Zone(Point center, Point size, const Outfit *outfit, double scrollY = 0.);

		double ScrollY() const;

	private:
		double scrollY = 0.;
	};

	enum class ShopPane : int {
		Main,
		Sidebar,
		Info
	};


private:
	bool DoScroll(double dy);
	bool SetScrollToTop();
	bool SetScrollToBottom();
	void SideSelect(int count);
	void SideSelect(Ship *ship);
	void MainLeft();
	void MainRight();
	void MainUp();
	void MainDown();
	std::vector<Zone>::const_iterator Selected() const;
	std::vector<Zone>::const_iterator MainStart() const;
	std::map<const Ship *, std::vector<std::string>> FlightCheck() const;
	bool ShipCanBuy(const Ship *ship, const Outfit *outfit) const;
	bool ShipCanSell(const Ship *ship, const Outfit *outfit) const;
	void DrawOutfit(const Outfit &outfit, const Point &center, bool isSelected, bool isOwned) const;
	bool IsLicense(const std::string &name) const;
	bool HasLicense(const std::string &name) const;
	std::string LicenseName(const std::string &name) const;
	// Check if the given point is within the button zone, and if so return the
	// letter of the button (or ' ' if it's not on a button).
	char CheckButton(int x, int y);

private:
	Editor &editor;
	ShipEditor &shipEditor;

	Ship *ship = nullptr;

	// The currently selected Outfit, for the OutfitterPanel.
	const Outfit *selectedOutfit = nullptr;

	double mainScroll = 0.;
	double sidebarScroll = 0.;
	double infobarScroll = 0.;
	double maxMainScroll = 0.;
	double maxSidebarScroll = 0.;
	double maxInfobarScroll = 0.;
	ShopPane activePane = ShopPane::Main;
	int mainDetailHeight = 0;
	int sideDetailHeight = 0;
	bool scrollDetailsIntoView = false;
	double selectedTopY = 0.;
	bool sameSelectedTopY = false;
	char hoverButton = '\0';

	std::vector<Zone> zones;
	std::vector<ClickZone<std::string>> categoryZones;

	std::map<std::string, std::set<std::string>> catalog;
	const std::vector<std::string> &categories;
	static inline std::set<std::string> collapsed;

	ShipInfoDisplay shipInfo;
	OutfitInfoDisplay outfitInfo;

	mutable Point warningPoint;
	mutable std::string warningType;
};


#endif
