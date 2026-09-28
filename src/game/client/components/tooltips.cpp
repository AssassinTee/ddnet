#include "tooltips.h"

#include <base/time.h>

#include <game/client/tooltips.h>
#include <game/client/ui.h>

CTooltipsComponent::CTooltipsComponent()
{
	OnReset();
}

void CTooltipsComponent::OnReset()
{
	CTooltips::Tooltips().OnReset();
}

void CTooltipsComponent::OnRender()
{
	CTooltips::Tooltips().OnRender(Ui(), TextRender());
}

void CTooltipsComponent::DoToolTip(const void *pId, const CUIRect *pNearRect, const char *pText, float WidthHint)
{
	uintptr_t Id = reinterpret_cast<uintptr_t>(pId);
	const auto &[Entry, WasInserted] = CTooltips::Tooltips().m_Tooltips.emplace(Id, CTooltip{
												pId,
												*pNearRect,
												pText,
												WidthHint,
												false});
	CTooltip &Tooltip = Entry->second;

	if(!WasInserted)
	{
		Tooltip.m_Rect = *pNearRect; // update in case of window resize
		Tooltip.m_pText = pText; // update in case of language change
	}

	Tooltip.m_OnScreen = true;

	if(Ui()->HotItem() == Tooltip.m_pId)
	{
		CTooltips::Tooltips().SetActiveTooltip(Tooltip);
	}
}
