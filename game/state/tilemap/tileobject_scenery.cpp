#include "game/state/tilemap/tileobject_scenery.h"
#include "framework/image.h"
#include "framework/renderer.h"
#include "game/state/city/scenery.h"
#include "game/state/rules/city/scenerytiletype.h"
#include "game/state/tilemap/tilemap.h"

namespace OpenApoc
{

namespace
{
static const Colour COLOUR_TRANSPARENT_BLACK = {255, 255, 255, 128};
}

void TileObjectScenery::draw(Renderer &r, TileTransform &transform, Vec2<float> screenPosition,
                             TileViewMode mode, bool visible, int, bool, bool)
{
	const float zoom = transform.getZoom();
	// Mode isn't used as TileView::tileToScreenCoords already transforms according to the mode
	auto scenery = this->scenery.lock();
	if (!scenery)
	{
		LogError("Called with no owning scenery object");
		return;
	}
	// FIXME: If damaged use damaged tile sprites?
	auto &type = scenery->type;
	sp<Image> sprite;
	sp<Image> overlaySprite;
	Vec2<float> transformedScreenPos = screenPosition;
	switch (mode)
	{
		case TileViewMode::Isometric:
			sprite = type->sprite;
			overlaySprite = type->overlaySprite;
			// Pixel offsets are in unscaled screen space, so they have to follow the zoom
			transformedScreenPos -=
			    Vec2<float>{type->imageOffset.x * zoom, type->imageOffset.y * zoom};
			break;
		case TileViewMode::Strategy:
			sprite = type->strategySprite;
			// All strategy sprites so far are 8x8 so offset by 4 to draw from the center
			// FIXME: Not true for large sprites (2x2 UFOs?)
			if (sprite)
			{
				transformedScreenPos -= Vec2<float>{(float)sprite->size.x * 0.5f * zoom,
				                                    (float)sprite->size.y * 0.5f * zoom};
			}
			break;
		default:
			LogError("Unsupported view mode");
	}
	if (sprite)
	{
		if (visible)
		{
			Vec2<float> scaledSize{(float)sprite->size.x * zoom, (float)sprite->size.y * zoom};
			r.drawScaled(sprite, transformedScreenPos, scaledSize, Renderer::Scaler::Linear);
		}
		else
		{
			r.drawTinted(sprite, transformedScreenPos, COLOUR_TRANSPARENT_BLACK);
		}
	}
	// FIXME: Should be drawn at 'later' Z than scenery (IE on top of any vehicles on tile?)
	if (overlaySprite)
	{
		if (visible)
		{
			Vec2<float> scaledSize{(float)overlaySprite->size.x * zoom,
			                       (float)overlaySprite->size.y * zoom};
			r.drawScaled(overlaySprite, transformedScreenPos, scaledSize, Renderer::Scaler::Linear);
		}
		else
		{
			r.drawTinted(overlaySprite, transformedScreenPos, COLOUR_TRANSPARENT_BLACK);
		}
	}
}

TileObjectScenery::~TileObjectScenery() = default;

TileObjectScenery::TileObjectScenery(TileMap &map, sp<Scenery> scenery)
    : TileObject(map, Type::Scenery, Vec3<float>{1, 1, 1}), scenery(scenery)
{
}

sp<Scenery> TileObjectScenery::getOwner() const
{
	auto s = this->scenery.lock();
	if (!s)
	{
		LogError("Owning scenery object disappeared");
	}
	return s;
}

sp<VoxelMap> TileObjectScenery::getVoxelMap(Vec3<int>, bool) const
{
	return this->getOwner()->type->voxelMap;
}

Vec3<float> TileObjectScenery::getPosition() const
{
	auto s = this->scenery.lock();
	if (!s)
	{
		LogError("Called with no owning scenery object");
		return {0, 0, 0};
	}
	return s->getPosition();
}

float TileObjectScenery::getZOrder() const
{
	// FIXME: Hack to force 'scenery' objects to be half-a-tile down in Z
	// The formula to calculate "3.5f" is: (tile_x + tile_y + tile_z) / tile_z /2
	return getCenter().z - 3.5f + (float)getType() / 1000.0f;
}

void TileObjectScenery::setPosition(Vec3<float> newPosition)
{
	TileObject::setPosition(newPosition);
	map.clearPathCaches();
	owningTile->updateCityscapeParameters();
}

void TileObjectScenery::removeFromMap()
{
	bool requireRecalc = owningTile != nullptr;
	auto prevOwningTile = owningTile;

	TileObject::removeFromMap();

	if (requireRecalc)
	{
		prevOwningTile->updateCityscapeParameters();
	}
}

void TileObjectScenery::addToDrawnTiles(Tile *tile)
{
	auto sc = scenery.lock();
	Vec3<int> maxCoords = {-1, -1, -1};
	for (auto &intersectingTile : intersectingTiles)
	{
		int x = intersectingTile->position.x;
		int y = intersectingTile->position.y;
		int z = intersectingTile->position.z;

		// Map parts are drawn in the topmost tile their head pops into
		// Otherwise, they can only be drawn in it if it's their owner tile
		if (maxCoords.z * 1000 + maxCoords.x + maxCoords.y < z * 1000 + x + y &&
		    sc->currentPosition.z + (float)sc->type->height / 16.1f >= (float)z)
		{
			tile = intersectingTile;
			maxCoords = {x, y, z};
		}
	}
	TileObject::addToDrawnTiles(tile);
}

} // namespace OpenApoc
