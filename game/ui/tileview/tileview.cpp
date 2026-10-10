#include "game/ui/tileview/tileview.h"
#include "framework/configfile.h"
#include "framework/event.h"
#include "framework/framework.h"
#include "framework/keycodes.h"
#include "framework/renderer.h"
#include "framework/sound.h"
#include "game/state/battle/battle.h"

namespace OpenApoc
{

namespace
{
// Scales a single tile size component by the zoom factor, keeping it at 1 or above so
// that code dividing by a tile size can never divide by zero
int scaleTileSizeComponent(int value, float zoom)
{
	int scaled = (int)((float)value * zoom);
	return scaled > 0 ? scaled : 1;
}
} // namespace

const std::vector<float> TileView::ZOOM_LEVELS = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f};

TileView::TileView(TileMap &map, Vec3<int> isoTileSize, Vec2<int> stratTileSize,
                   TileViewMode initialMode)
    : Stage(), map(map), isoTileSize(isoTileSize), stratTileSize(stratTileSize),
      isoTileSizeBase(isoTileSize), stratTileSizeBase(stratTileSize), viewMode(initialMode),
      dpySize(fw().displayGetWidth(), fw().displayGetHeight()),
      strategyViewBoxColour(212, 176, 172, 255), strategyViewBoxThickness(2.0f),
      selectedTilePosition(0, 0, 0), maxZDraw(map.size.z), centerPos(0, 0, 0),
      isoScrollSpeed(0.5, 0.5), stratScrollSpeed(2.0f, 2.0f)
{
	LogInfo("dpySize: {0}", dpySize);
}

TileView::~TileView() = default;

float TileView::getZoom() const { return this->zoomLevel; }

int TileView::getZoomStep() const
{
	int step = 0;
	float smallestDelta = this->zoomLevel > ZOOM_LEVELS[0] ? this->zoomLevel - ZOOM_LEVELS[0]
	                                                       : ZOOM_LEVELS[0] - this->zoomLevel;
	for (int i = 1; i < (int)ZOOM_LEVELS.size(); i++)
	{
		float delta = this->zoomLevel > ZOOM_LEVELS[i] ? this->zoomLevel - ZOOM_LEVELS[i]
		                                               : ZOOM_LEVELS[i] - this->zoomLevel;
		if (delta < smallestDelta)
		{
			smallestDelta = delta;
			step = i;
		}
	}
	return step;
}

void TileView::setZoomLevel(float zoom)
{
	// Snap the requested value to the nearest of the supported zoom steps
	int step = 0;
	float smallestDelta = zoom > ZOOM_LEVELS[0] ? zoom - ZOOM_LEVELS[0] : ZOOM_LEVELS[0] - zoom;
	for (int i = 1; i < (int)ZOOM_LEVELS.size(); i++)
	{
		float delta = zoom > ZOOM_LEVELS[i] ? zoom - ZOOM_LEVELS[i] : ZOOM_LEVELS[i] - zoom;
		if (delta < smallestDelta)
		{
			smallestDelta = delta;
			step = i;
		}
	}
	if (ZOOM_LEVELS[step] == this->zoomLevel)
	{
		return;
	}

	// centerPos is stored in tile coordinates, so it is unaffected by the change of
	// tile size and the view keeps showing the same tile in the middle of the screen
	this->zoomLevel = ZOOM_LEVELS[step];
	this->isoTileSize = {scaleTileSizeComponent(this->isoTileSizeBase.x, this->zoomLevel),
	                     scaleTileSizeComponent(this->isoTileSizeBase.y, this->zoomLevel),
	                     scaleTileSizeComponent(this->isoTileSizeBase.z, this->zoomLevel)};
	this->stratTileSize = {scaleTileSizeComponent(this->stratTileSizeBase.x, this->zoomLevel),
	                       scaleTileSizeComponent(this->stratTileSizeBase.y, this->zoomLevel)};
	LogInfo("Zoom set to {0}", this->zoomLevel);
}

void TileView::zoomIn()
{
	int step = this->getZoomStep();
	if (step + 1 < (int)ZOOM_LEVELS.size())
	{
		this->setZoomLevel(ZOOM_LEVELS[step + 1]);
	}
}

void TileView::zoomOut()
{
	int step = this->getZoomStep();
	if (step > 0)
	{
		this->setZoomLevel(ZOOM_LEVELS[step - 1]);
	}
}

void TileView::begin() { autoScroll = config().getBool("Options.Misc.AutoScroll"); }

void TileView::pause()
{
	scrollUpKB = false;
	scrollDownKB = false;
	scrollLeftKB = false;
	scrollRightKB = false;
}

void TileView::resume() { autoScroll = config().getBool("Options.Misc.AutoScroll"); }

void TileView::finish() {}

void TileView::eventOccurred(Event *e)
{
	if (e->type() == EVENT_KEY_DOWN)
	{
		switch (e->keyboard().KeyCode)
		{
			case SDLK_F1:
				debugHotkeyMode = !debugHotkeyMode;
				LogWarning("DEBUG MODE {0}", debugHotkeyMode);
				break;
			case SDLK_UP:
				scrollUpKB = true;
				break;
			case SDLK_DOWN:
				scrollDownKB = true;
				break;
			case SDLK_LEFT:
				scrollLeftKB = true;
				break;
			case SDLK_RIGHT:
				scrollRightKB = true;
				break;
			case SDLK_s:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.y < (map.size.y - 1))
						this->setSelectedTilePosition({selectedTilePosition.x,
						                               selectedTilePosition.y + 1,
						                               selectedTilePosition.z});
				}
				else
				{
					scrollDownKB = true;
				}
				break;
			case SDLK_w:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.y > 0)
						this->setSelectedTilePosition({selectedTilePosition.x,
						                               selectedTilePosition.y - 1,
						                               selectedTilePosition.z});
				}
				else
				{
					scrollUpKB = true;
				}
				break;
			case SDLK_a:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.x > 0)
						this->setSelectedTilePosition({selectedTilePosition.x - 1,
						                               selectedTilePosition.y,
						                               selectedTilePosition.z});
				}
				else
				{
					scrollLeftKB = true;
				}
				break;
			case SDLK_d:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.x < (map.size.x - 1))
						this->setSelectedTilePosition({selectedTilePosition.x + 1,
						                               selectedTilePosition.y,
						                               selectedTilePosition.z});
				}
				else
				{
					scrollRightKB = true;
				}
				break;
			case SDLK_r:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.z < (map.size.z - 1))
						this->setSelectedTilePosition({selectedTilePosition.x,
						                               selectedTilePosition.y,
						                               selectedTilePosition.z + 1});
				}
				break;
			case SDLK_f:
				if (debugHotkeyMode)
				{
					if (selectedTilePosition.z > 0)
						this->setSelectedTilePosition({selectedTilePosition.x,
						                               selectedTilePosition.y,
						                               selectedTilePosition.z - 1});
				}
				break;
		}
	}
	else if (e->type() == EVENT_KEY_UP)
	{
		switch (e->keyboard().KeyCode)
		{
			case SDLK_UP:
				scrollUpKB = false;
				break;
			case SDLK_DOWN:
				scrollDownKB = false;
				break;
			case SDLK_LEFT:
				scrollLeftKB = false;
				break;
			case SDLK_RIGHT:
				scrollRightKB = false;
				break;
			case SDLK_w:
				scrollUpKB = false;
				break;
			case SDLK_s:
				scrollDownKB = false;
				break;
			case SDLK_a:
				scrollLeftKB = false;
				break;
			case SDLK_d:
				scrollRightKB = false;
				break;
		}
	}
	else if (e->type() == EVENT_WINDOW_DEACTIVATE)
	{
		scrollUpKB = false;
		scrollDownKB = false;
		scrollLeftKB = false;
		scrollRightKB = false;
	}
	else if (e->type() == EVENT_FINGER_MOVE)
	{
		// FIXME: Review this code for sanity
		if (e->finger().IsPrimary)
		{
			Vec3<float> deltaPos(e->finger().DeltaX, e->finger().DeltaY, 0);
			if (this->viewMode == TileViewMode::Isometric)
			{
				deltaPos.x /= isoTileSize.x;
				deltaPos.y /= isoTileSize.y;
				Vec3<float> isoDelta(deltaPos.x + deltaPos.y, deltaPos.y - deltaPos.x, 0);
				deltaPos = isoDelta;
			}
			else
			{
				deltaPos.x /= stratTileSize.x;
				deltaPos.y /= stratTileSize.y;
			}
			Vec3<float> newPos = this->centerPos - deltaPos;
			this->setScreenCenterTile(newPos);
		}
	}
	else if (e->type() == EVENT_MOUSE_MOVE)
	{
		scrollLeftM = autoScroll && e->mouse().X < MOUSE_SCROLL_MARGIN;
		scrollRightM = autoScroll && e->mouse().X >= dpySize.x - MOUSE_SCROLL_MARGIN;
		scrollUpM = autoScroll && e->mouse().Y < MOUSE_SCROLL_MARGIN;
		scrollDownM = autoScroll && e->mouse().Y >= dpySize.y - MOUSE_SCROLL_MARGIN;
	}
	else if (e->type() == EVENT_MOUSE_SCROLL)
	{
		// Wheel up zooms in, wheel down zooms out. BattleTileView and CityTileView both
		// forward their events here, so this covers the battlescape and the cityscape.
		if (e->mouse().WheelVertical > 0)
		{
			this->zoomIn();
		}
		else if (e->mouse().WheelVertical < 0)
		{
			this->zoomOut();
		}
	}
}

bool TileView::isTransition() { return false; }

void TileView::setViewMode(TileViewMode newMode) { this->viewMode = newMode; }

TileViewMode TileView::getViewMode() const { return this->viewMode; }

Vec2<int> TileView::getScreenOffset() const
{
	Vec2<float> screenOffset = this->tileToScreenCoords(this->centerPos);

	return Vec2<int>{dpySize.x / 2 - screenOffset.x, dpySize.y / 2 - screenOffset.y};
}

void TileView::setScreenCenterTile(Vec3<float> center)
{
	fw().soundBackend->setListenerPosition({center.x, center.y, map.size.z / 2});
	Vec3<float> clampedCenter;
	if (center.x < 0.0f)
		clampedCenter.x = 0.0f;
	else if (center.x > map.size.x)
		clampedCenter.x = map.size.x;
	else
		clampedCenter.x = center.x;
	if (center.y < 0.0f)
		clampedCenter.y = 0.0f;
	else if (center.y > map.size.y)
		clampedCenter.y = map.size.y;
	else
		clampedCenter.y = center.y;
	if (center.z < 0.0f)
		clampedCenter.z = 0.0f;
	else if (center.z > map.size.z)
		clampedCenter.z = map.size.z;
	else
		clampedCenter.z = center.z;

	this->centerPos = clampedCenter;
}

void TileView::setScreenCenterTile(Vec2<float> center)
{
	this->setScreenCenterTile(Vec3<float>{center.x, center.y, 1});
}

Vec3<int> TileView::getSelectedTilePosition() { return selectedTilePosition; }

void TileView::setSelectedTilePosition(Vec3<int> newPosition)
{
	selectedTilePosition = newPosition;
	if (selectedTilePosition.x < 0)
		selectedTilePosition.x = 0;
	if (selectedTilePosition.y < 0)
		selectedTilePosition.y = 0;
	if (selectedTilePosition.z < 0)
		selectedTilePosition.z = 0;
	if (selectedTilePosition.x >= map.size.x)
		selectedTilePosition.x = map.size.x - 1;
	if (selectedTilePosition.y >= map.size.y)
		selectedTilePosition.y = map.size.y - 1;
	if (selectedTilePosition.z >= map.size.z)
		selectedTilePosition.z = map.size.z - 1;
}

void TileView::applyScrolling()
{
	Vec3<float> newPos = this->centerPos;
	if (this->viewMode == TileViewMode::Isometric)
	{
		if (scrollLeftKB || scrollLeftM)
		{
			newPos.x -= isoScrollSpeed.x;
			newPos.y += isoScrollSpeed.y;
		}
		if (scrollRightKB || scrollRightM)
		{
			newPos.x += isoScrollSpeed.x;
			newPos.y -= isoScrollSpeed.y;
		}
		if (scrollUpKB || scrollUpM)
		{
			newPos.y -= isoScrollSpeed.y;
			newPos.x -= isoScrollSpeed.x;
		}
		if (scrollDownKB || scrollDownM)
		{
			newPos.y += isoScrollSpeed.y;
			newPos.x += isoScrollSpeed.x;
		}
	}
	else if (this->viewMode == TileViewMode::Strategy)
	{
		if (scrollLeftKB || scrollLeftM)
			newPos.x -= stratScrollSpeed.x;
		if (scrollRightKB || scrollRightM)
			newPos.x += stratScrollSpeed.x;
		if (scrollUpKB || scrollUpM)
			newPos.y -= stratScrollSpeed.y;
		if (scrollDownKB || scrollDownM)
			newPos.y += stratScrollSpeed.y;
	}
	else
	{
		LogError("Unknown view mode");
	}

	this->setScreenCenterTile(newPos);
}

void TileView::renderStrategyOverlay(Renderer &r)
{
	if (this->viewMode == TileViewMode::Strategy)
	{
		Vec2<float> centerIsoScreenPos = this->tileToScreenCoords(
		    Vec3<float>{this->centerPos.x, this->centerPos.y, 0}, TileViewMode::Isometric);

		/* Draw the rectangle of where the isometric view would be */
		Vec2<float> topLeftIsoScreenPos = centerIsoScreenPos;
		topLeftIsoScreenPos.x -= dpySize.x / 2;
		topLeftIsoScreenPos.y -= dpySize.y / 2;

		Vec2<float> topRightIsoScreenPos = centerIsoScreenPos;
		topRightIsoScreenPos.x += dpySize.x / 2;
		topRightIsoScreenPos.y -= dpySize.y / 2;

		Vec2<float> bottomLeftIsoScreenPos = centerIsoScreenPos;
		bottomLeftIsoScreenPos.x -= dpySize.x / 2;
		bottomLeftIsoScreenPos.y += dpySize.y / 2;

		Vec2<float> bottomRightIsoScreenPos = centerIsoScreenPos;
		bottomRightIsoScreenPos.x += dpySize.x / 2;
		bottomRightIsoScreenPos.y += dpySize.y / 2;

		Vec3<float> topLeftIsoTilePos =
		    this->screenToTileCoords(topLeftIsoScreenPos, 0.0f, TileViewMode::Isometric);
		Vec3<float> topRightIsoTilePos =
		    this->screenToTileCoords(topRightIsoScreenPos, 0.0f, TileViewMode::Isometric);
		Vec3<float> bottomLeftIsoTilePos =
		    this->screenToTileCoords(bottomLeftIsoScreenPos, 0.0f, TileViewMode::Isometric);
		Vec3<float> bottomRightIsoTilePos =
		    this->screenToTileCoords(bottomRightIsoScreenPos, 0.0f, TileViewMode::Isometric);

		Vec2<float> topLeftRectPos = this->tileToOffsetScreenCoords(topLeftIsoTilePos);
		Vec2<float> topRightRectPos = this->tileToOffsetScreenCoords(topRightIsoTilePos);
		Vec2<float> bottomLeftRectPos = this->tileToOffsetScreenCoords(bottomLeftIsoTilePos);
		Vec2<float> bottomRightRectPos = this->tileToOffsetScreenCoords(bottomRightIsoTilePos);

		r.drawLine(topLeftRectPos, topRightRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);
		r.drawLine(topRightRectPos, bottomRightRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);

		r.drawLine(bottomRightRectPos, bottomLeftRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);
		r.drawLine(bottomLeftRectPos, topLeftRectPos, this->strategyViewBoxColour,
		           this->strategyViewBoxThickness);
	}
}

void TileView::update() { applyScrolling(); }
}; // namespace OpenApoc
