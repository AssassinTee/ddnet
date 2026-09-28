#ifndef GAME_CLIENT_TOOLTIPS_H
#define GAME_CLIENT_TOOLTIPS_H

#include <engine/textrender.h>

#include <game/client/components/tooltips.h>
#include <game/client/ui_rect.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>

struct CTooltip
{
	const void *m_pId;
	CUIRect m_Rect;
	const char *m_pText;
	float m_WidthHint;
	bool m_OnScreen; // used to know if the tooltip should be rendered.
};

class CUi;
class ITextRender;

/**
 * A component that manages and renders UI tooltips.
 *
 * Should be among the last components to render.
 */
class CTooltips
{
	CTooltips();
	std::unordered_map<uintptr_t, CTooltip> m_Tooltips;
	std::optional<std::reference_wrapper<CTooltip>> m_ActiveTooltip;
	std::optional<std::reference_wrapper<CTooltip>> m_PreviousTooltip;
	int64_t m_HoverTime;

	/**
	 * @param Tooltip A reference to the tooltip that should be active.
	 */
	void SetActiveTooltip(CTooltip &Tooltip);

	inline void ClearActiveTooltip();
	void OnReset();
	void OnRender(CUi *pUi, ITextRender *pTextRender);

public:
	CTooltips(const CTooltips &) = delete;
	CTooltips &operator=(const CTooltips &) = delete;
	static CTooltips &Tooltips()
	{
		static CTooltips s_Tooltips;
		return s_Tooltips;
	}

	void DoToolTip(CUi *pUi, const void *pId, const CUIRect *pNearRect, const char *pText, float WidthHint);
};

#endif
