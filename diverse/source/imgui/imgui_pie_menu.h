#pragma once

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/fwd.hpp>
#include <iostream>

namespace diverse
{
    struct PieMenuContext
	{
		static const int	c_iMaxPieMenuStack = 8;
		static const int	c_iMaxPieItemCount = 12;
		static const int	c_iRadiusEmpty = 30;
		static const int	c_iRadiusMin = 30;
		static const int	c_iMinItemCount = 3;
		static const int	c_iMinItemCountPerLevel = 3;

		struct PieMenu
		{
			int				m_iCurrentIndex;
			float			m_fMaxItemSqrDiameter;
			float			m_fLastMaxItemSqrDiameter;
			int				m_iHoveredItem;
			int				m_iLastHoveredItem;
			int				m_iClickedItem;
			bool			m_oItemIsSubMenu[c_iMaxPieItemCount];
			ImVector<char>	m_oItemNames[c_iMaxPieItemCount];
			ImVec2			m_oItemSizes[c_iMaxPieItemCount];
		};

		PieMenuContext()
		{
			m_iCurrentIndex = -1;
			m_iLastFrame = 0;
		}

		PieMenu				m_oPieMenuStack[c_iMaxPieMenuStack];
		int					m_iCurrentIndex;
		int					m_iMaxIndex;
		int					m_iLastFrame;
		ImVec2				m_oCenter;
		int					m_iMouseButton;
		bool				m_bClose;
	};
	bool BeginPiePopup(const char* pName, int iMouseButton = 0);
	void EndPiePopup();

	bool PieMenuItem(const char* pName, bool bEnabled = true);
	bool BeginPieMenu(const char* pName, bool bEnabled = true);
	void EndPieMenu();

}