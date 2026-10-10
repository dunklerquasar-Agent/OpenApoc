#include "game/state/tilemap/tileobject_battleitem.h"
#include "framework/renderer.h"
#include "game/state/battle/battleitem.h"
#include "game/state/rules/aequipmenttype.h"
#include "game/state/shared/aequipment.h"
#include "game/state/tilemap/tilemap.h"
#include "library/line.h"

namespace OpenApoc
{

void TileObjectBattleItem::draw(Renderer &r, TileTransform &transform, Vec2<float> screenPosition,
                                 TileViewMode mode, bool visible, int, bool, bool)
{
	// Mode isn't used as TileView::tileToScreenCoords already transforms according to the mode
	std::ignore = mode;
	const float zoom = transform.getZoom();

	auto item = this->item.lock();
	if (!item)
	{
		LogError("Called with no owning item object");
		return;
	}
	Vec2<float> transformedScreenPos = screenPosition;
	sp<Image> sprite;
	switch (mode)
	{
		case TileViewMode::Isometric:
			sprite = item->item->type->dropped_sprite;
			// Pixel offsets are in unscaled screen space, so they follow the zoom
			transformedScreenPos -= Vec2<float>{item->item->type->dropped_offset.x * zoom,
			                                    item->item->type->dropped_offset.y * zoom};
			break;
		case TileViewMode::Strategy:
		{
			if (item->falling || !visible)
				break;
			sprite = item->strategySprite;
			if (sprite)
			{
				transformedScreenPos -= Vec2<float>{(float)sprite->size.x * 0.5f * zoom,
				                                    (float)sprite->size.y * 0.5f * zoom};
			}
			break;
		}
		default:
			LogError("Unsupported view mode");
	}
	if (sprite)
		drawTinted(r, sprite, transformedScreenPos, visible, zoom);
}

TileObjectBattleItem::~TileObjectBattleItem() = default;

TileObjectBattleItem::TileObjectBattleItem(TileMap &map, sp<BattleItem> item)
    // 1.0f bounds on z axis provides for same image offsets of all battlescape sprites
    : TileObject(map, Type::Item, Vec3<float>{0.0f, 0.0f, 1.0f}), item(item)
{
}

sp<BattleItem> TileObjectBattleItem::getItem()
{
	auto i = item.lock();
	if (!i)
	{
		LogError("Item disappeared");
		return nullptr;
	}
	return i;
}

Vec3<float> TileObjectBattleItem::getPosition() const
{
	auto p = this->item.lock();
	if (!p)
	{
		LogError("Called with no owning item object");
		return {0, 0, 0};
	}
	return p->getPosition();
}

float TileObjectBattleItem::getZOrder() const { return getPosition().z - 7.0f; }

} // namespace OpenApoc
