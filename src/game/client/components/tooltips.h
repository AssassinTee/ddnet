#ifndef GAME_CLIENT_COMPONENTS_TOOLTIPS_H
#define GAME_CLIENT_COMPONENTS_TOOLTIPS_H

#include <game/client/component.h>
#include <game/client/ui_rect.h>

/**
 * A component that manages and renders UI tooltips.
 *
 * Should be among the last components to render.
 */
class CTooltipsComponent : public CComponent
{
public:
	CTooltipsComponent();
	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRender() override;
	void DoToolTip(const void *pId, const CUIRect *pNearRect, const char *pText, float WidthHint = -1.0f);
};

#endif
