#include "stdafx.h"
#include <functional>

#include "UIMainIngameWnd.h"
#include "UIMessagesWindow.h"
#include "UIInventoryUtilities.h"
#include "UIScrollView.h"
#include "UIColorAnimatorWrapper.h"
#include "UIXmlInit.h"
#include "UIPdaMsgListItem.h"
#include "../UIZoneMap.h"
#include "../../Include/xrRender/Kinematics.h"

#include <dinput.h>
#include "../actor.h"
#include "../HUDManager.h"
#include "../PDA.h"
#include "../player_hud.h"
#include "../character_info.h"
#include "../inventory.h"
#include "../UIGameSP.h"
#include "../weaponmagazined.h"
#include "../missile.h"
#include "../Grenade.h"

#include "../game_cl_base.h"
#include "../level.h"

#include "../torch.h"
#include "../date_time.h"


#include "../ActorState.h"
#include "../alife_registry_wrappers.h"
#include "../actorcondition.h"
#include "../string_table.h"
#include "../clsid_game.h"
#include "../mounted_turret.h"
#include "map_hint.h"
#include "../game_news.h"


using namespace InventoryUtilities;

const u32	g_clWhite					= 0xffffffff;

#define		DEFAULT_MAP_SCALE			1.f

#define		C_SIZE						0.025f
#define		NEAR_LIM					0.5f

#define		SHOW_INFO_SPEED				0.5f
#define		HIDE_INFO_SPEED				10.f
#define		C_ON_ENEMY					D3DCOLOR_XRGB(0xff,0,0)
#define		C_DEFAULT					D3DCOLOR_XRGB(0xff,0xff,0xff)

#define		MAININGAME_XML				"maingame.xml"

CUIMainIngameWnd::CUIMainIngameWnd()
{
	m_pActor					= NULL;
	m_pWeapon					= NULL;
	m_pGrenade					= NULL;
	m_pItem						= NULL;
	UIZoneMap					= new CUIZoneMap();
	m_pPickUpItem				= NULL;
}

#include "UIProgressShape.h"
extern CUIProgressShape* g_MissileForceShape;

CUIMainIngameWnd::~CUIMainIngameWnd()
{
	DestroyFlashingIcons		();
	xr_delete					(UIZoneMap);
	HUD_SOUND_ITEM::DestroySound		(m_contactSnd);
	xr_delete					(g_MissileForceShape);
}

void CUIMainIngameWnd::Init()
{
	CUIXml						uiXml;

	string128		XmlName;
	xr_sprintf		(XmlName, "maingame_%d.xml", ui_hud_type);

	uiXml.Load					(CONFIG_PATH, UI_PATH, XmlName);
	
	CUIXmlInit					xml_init;
	CUIWindow::SetWndRect		(Frect().set(0,0, UI_BASE_WIDTH, UI_BASE_HEIGHT));

	Enable(false);


	AttachChild					(&UIStaticHealth);
	xml_init.InitStatic			(uiXml, "static_health", 0, &UIStaticHealth);

	AttachChild					(&UIWeaponBack);
	xml_init.InitStatic			(uiXml, "static_weapon", 0, &UIWeaponBack);

	UIWeaponBack.AttachChild	(&UIWeaponSignAmmo);
	xml_init.InitStatic			(uiXml, "static_ammo", 0, &UIWeaponSignAmmo);
	UIWeaponSignAmmo.SetElipsis	(CUIStatic::eepEnd, 2);

	UIWeaponBack.AttachChild	(&UIWeaponIcon);
	xml_init.InitStatic			(uiXml, "static_wpn_icon", 0, &UIWeaponIcon);

	UIWeaponBack.AttachChild	(&UIWeaponFiremode);
	xml_init.InitStatic			(uiXml, "static_wpn_firemode", 0, &UIWeaponFiremode);

	UIWeaponIcon.SetShader		(GetEquipmentIconsShader());
	UIWeaponIcon_rect			= UIWeaponIcon.GetWndRect();

	AttachChild(&UIStaticTime);
	xml_init.InitStatic(uiXml, "static_time", 0, &UIStaticTime);
	AttachChild(&UIStaticDate);
	xml_init.InitStatic(uiXml, "static_date", 0, &UIStaticDate);
	AttachChild(&UIStaticTorch);
	xml_init.InitStatic(uiXml, "static_flashlight", 0, &UIStaticTorch);
	AttachChild(&UIStaticTurret);
	xml_init.InitStatic(uiXml, "static_turret", 0, &UIStaticTurret);
	AttachChild(&UIActorStateIcons);
	UIActorStateIcons.Init();

	
	//---------------------------------------------------------
	AttachChild					(&UIPickUpItemIcon);
	xml_init.InitStatic			(uiXml, "pick_up_item", 0, &UIPickUpItemIcon);
	UIPickUpItemIcon.SetShader	(GetEquipmentIconsShader());

	m_iPickUpItemIconWidth		= UIPickUpItemIcon.GetWidth();
	m_iPickUpItemIconHeight		= UIPickUpItemIcon.GetHeight();
	m_iPickUpItemIconX			= UIPickUpItemIcon.GetWndRect().left;
	m_iPickUpItemIconY			= UIPickUpItemIcon.GetWndRect().top;
	//---------------------------------------------------------

	UIWeaponIcon.Enable			(false);

	UIZoneMap->Init				();
	UIZoneMap->SetScale			(DEFAULT_MAP_SCALE);

	xml_init.InitStatic					(uiXml, "static_pda_online", 0, &UIPdaOnline);
	UIZoneMap->Background().AttachChild	(&UIPdaOnline);

	//?????? ????????? ????????
	UIStaticHealth.AttachChild	(&UIHealthBar);
	xml_init.InitProgressBar	(uiXml, "progress_bar_health", 0, &UIHealthBar);

	UIStaticTorch.AttachChild(&UIFlashlightBar);
	xml_init.InitProgressBar(uiXml, "progress_bar_flashlight", 0, &UIFlashlightBar);
	UIFlashlightBar.Show(false);
	UIStaticTorch.Show(false);

	UIStaticTurret.AttachChild(&UITurretBar);
	xml_init.InitProgressBar(uiXml, "progress_bar_turret", 0, &UITurretBar);
	UITurretBar.Show(false);
	UIStaticTurret.Show(false);

	// ?????????, ??????? ????????? ??? ????????? ??????? ?? ??????
	AttachChild					(&UIStaticQuickHelp);
	xml_init.InitTextWnd			(uiXml, "quick_info", 0, &UIStaticQuickHelp);

	uiXml.SetLocalRoot			(uiXml.GetRoot());

	m_UIIcons					= new CUIScrollView(); m_UIIcons->SetAutoDelete(true);
	xml_init.InitScrollView		(uiXml, "icons_scroll_view", 0, m_UIIcons);
	AttachChild					(m_UIIcons);

	// ????????? ?????? 
	xml_init.InitStatic			(uiXml, "invincible_static", 0, &UIInvincibleIcon);
	UIInvincibleIcon.Show		(false);

	shared_str warningStrings[7] = 
	{	
		"jammed",
		"radiation",
		"wounds",
		"starvation",
		"thirst",
		"fatigue",
		"invincible"
	};

	// ????????? ????????? ???????? ??? ???????????
	EWarningIcons j = ewiWeaponJammed;
	while (j < ewiInvincible)
	{
		// ?????? ?????? ??????? ??? ??????? ??????????
		shared_str cfgRecord = pSettings->r_string("la_main_ingame_indicators_thresholds", *warningStrings[static_cast<int>(j) - 1]);
		u32 count = _GetItemCount(*cfgRecord);

		R_ASSERT3(count==3, "Item count in parameter '%s' in [la_main_ingame_indicators_thresholds] should be 3.", *warningStrings[static_cast<int>(j) - 1]);

		char	singleThreshold[8];
		float	f = 0;
		for (u32 k = 0; k < count; ++k)
		{
			_GetItem(*cfgRecord, k, singleThreshold);
			sscanf(singleThreshold, "%f", &f);

			m_Thresholds[j].push_back(f);
		}

		j = static_cast<EWarningIcons>(j + 1);
	}


	// Flashing icons initialize
	uiXml.SetLocalRoot						(uiXml.NavigateToNode("flashing_icons"));
	InitFlashingIcons						(&uiXml);

	uiXml.SetLocalRoot						(uiXml.GetRoot());
	
	AttachChild								(&UICarPanel);
	xml_init.InitWindow						(uiXml, "car_panel", 0, &UICarPanel);
	Frect wndrect = UICarPanel.GetWndRect();
	UICarPanel.Init							(wndrect.x1, wndrect.y1, wndrect.width(), wndrect.height());

	AttachChild								(&UIMotionIcon);
	UIMotionIcon.Init						();

	HUD_SOUND_ITEM::LoadSound					("maingame_ui", "snd_new_contact"		, m_contactSnd		, SOUND_TYPE_IDLE);
}

void CUIMainIngameWnd::Draw()
{
	if(!m_pActor) return;

	UIMotionIcon.SetNoise		((s16)(0xffff&iFloor(m_pActor->m_snd_noise*100.0f)));
	CUIWindow::Draw				();
	UIZoneMap->Render			();			

	RenderQuickInfos			();
}

void CUIMainIngameWnd::SetAmmoIcon (const shared_str& sect_name)
{
	if ( !sect_name.size() )
	{
		UIWeaponIcon.Show			(false);
		return;
	};

	UIWeaponIcon.Show			(true);
	//properties used by inventory menu
	Frect texture_rect;
	texture_rect.x1					= pSettings->r_float( sect_name,  "inv_grid_x")		*INV_GRID_WIDTH;
	texture_rect.y1					= pSettings->r_float( sect_name,  "inv_grid_y")		*INV_GRID_HEIGHT;
	texture_rect.x2					= pSettings->r_float( sect_name, "inv_grid_width")	*INV_GRID_WIDTH;
	texture_rect.y2					= pSettings->r_float( sect_name, "inv_grid_height")	*INV_GRID_HEIGHT;
	texture_rect.rb.add				(texture_rect.lt);

	UIWeaponIcon.GetUIStaticItem().SetTextureRect(texture_rect);
	UIWeaponIcon.SetStretchTexture(true);

	// now perform only width scale for ammo, which (W)size >2
	// all others ammo (1x1, 1x2) will be not scaled (original picture)
	float w = ((texture_rect.width() < 2.f*INV_GRID_WIDTH)?0.5f:1.f)*UIWeaponIcon_rect.width();
	float h = UIWeaponIcon_rect.height();//1 cell

	float x = UIWeaponIcon_rect.x1;
	if (texture_rect.width() < 2.f*INV_GRID_WIDTH)
		x += ( UIWeaponIcon_rect.width() - w) / 2.0f;

	UIWeaponIcon.SetWndPos	(Fvector2().set(x, UIWeaponIcon_rect.y1));
	
	UIWeaponIcon.SetWidth	(w);
	UIWeaponIcon.SetHeight	(h);
};

void CUIMainIngameWnd::Update()
{
	m_pActor = smart_cast<CActor*>(Level().CurrentViewEntity());
	if (!m_pActor) 
	{
		m_pItem					= NULL;
		m_pWeapon				= NULL;
		m_pGrenade				= NULL;
		CUIWindow::Update		();
		return;
	}

	if( !Device.dwFrame%30 )
	{
			string256				text_str;
			CPda* _pda	= m_pActor->GetPDA();
			u32 _cn		= 0;
			if(_pda && 0!= (_cn=_pda->ActiveContactsNum()) )
			{
				xr_sprintf(text_str, "%d", _cn);
				if (g_SingleGameDifficulty == egdMaster){
					UIPdaOnline.TextItemControl()->SetText("");
				}
				else{
					UIPdaOnline.TextItemControl()->SetText(text_str);
				}
			}
			else
			{
				UIPdaOnline.TextItemControl()->SetText("");
			}
	};

	if( !(Device.dwFrame%5) )
	{

		if(!(Device.dwFrame%30))
		{
			bool b_God = (GodMode()||(!Game().local_player)) ? true : Game().local_player->testFlag(GAME_PLAYER_FLAG_INVINCIBLE);
			if(b_God)
				SetWarningIconColor	(ewiInvincible,0xffffffff);
			else
				SetWarningIconColor	(ewiInvincible,0x00ffffff);
		}

		UpdateActiveItemInfo				();

		EWarningIcons i					= ewiWeaponJammed;

		while (i < ewiInvincible)
		{
			float value = 0.f;
			EActorState NewState, InactiveState;

			switch (i)
			{
			case ewiWeaponJammed:
				if (m_pWeapon)
					value = 1 - m_pWeapon->GetConditionToShow();
				NewState = InactiveState = eJammedInactive;
				break;
			case ewiRadiation:
				value = m_pActor->conditions().GetRadiation();
				NewState = InactiveState = eRadiationInactive;
				break;
			case ewiWound:
				value = m_pActor->conditions().BleedingSpeed();
				NewState = InactiveState = eBleedingInactive;
				break;
			case ewiStarvation:
				value = 1 - m_pActor->conditions().GetSatiety();
				NewState = InactiveState = eHungerInactive;
				break;
			case ewiThirst:
				value = 1 - m_pActor->conditions().GetThirsty();
				NewState = InactiveState = eThirstInactive;
				break;
			case ewiPsyHealth:
				value = 1 - m_pActor->conditions().GetPsyHealth();
				NewState = InactiveState = ePsyHealthInactive;
				break;
			default:
				R_ASSERT(!"Unknown type of warning icon");
			}

			xr_vector<float>::reverse_iterator	rit;

			// ??????? ????????? ?? ?????? ???????????
			rit  = std::find(m_Thresholds[i].rbegin(), m_Thresholds[i].rend(), value);

			// ???? ??? ???, ?? ????? ????????? ??????? ???????? ()
			if (rit == m_Thresholds[i].rend())
				rit = std::find_if(m_Thresholds[i].rbegin(), m_Thresholds[i].rend(), std::bind2nd(std::less<float>(), value));

			// ??????????? ? ???????????? ???????? ???????
			float min = m_Thresholds[i].front();
			float max = m_Thresholds[i].back();

			if (rit != m_Thresholds[i].rend())
			{
				float v = *rit;

				if (fsimilar(min, v))
					NewState = EActorState(InactiveState + 3); // green
				else if (fsimilar(max, v))
					NewState = EActorState(InactiveState + 1); // red
				else
					NewState = EActorState(InactiveState + 2); // yellow
			}

			for (u8 j = 0; j < 4; j++)
			{
				EActorState Icon = EActorState(InactiveState + j);
				if (Icon == NewState)
					m_pActor->SetIconState(Icon, true);
				else
					m_pActor->SetIconState(Icon, false);
			}

			i = (EWarningIcons)(i + 1);
		}
	}

	// health&armor
	UIHealthBar.SetProgressPos		(m_pActor->GetfHealth()*100.0f);
	UIMotionIcon.SetPower			(m_pActor->conditions().GetPower()*100.0f);

	if (psHUD_Flags.test(HUD_SHOW_CLOCK))
	{
		if (!UIStaticTime.IsShown())
			UIStaticTime.Show(true);
		if (!UIStaticDate.IsShown())
			UIStaticDate.Show(true);
		UIStaticTime.TextItemControl()->SetText(*InventoryUtilities::GetGameTimeAsString(InventoryUtilities::etpTimeToMinutes));
		UIStaticDate.TextItemControl()->SetText(*InventoryUtilities::GetGameDateAsString(InventoryUtilities::edpDateToDay));
	}
	else
	{
		if (UIStaticTime.IsShown())
			UIStaticTime.Show(false);
		if (UIStaticDate.IsShown())
			UIStaticDate.Show(false);
	}

	CTorch *flashlight = m_pActor->GetCurrentTorch();
	if (flashlight)
	{
		if (flashlight->IsSwitchedOn())
		{
			if (!UIStaticTorch.IsShown())
				UIStaticTorch.Show(true);
			if (!UIFlashlightBar.IsShown())
				UIFlashlightBar.Show(true);
			UIFlashlightBar.SetProgressPos(100.0f * (((float)flashlight->GetBatteryStatus()) / ((float)flashlight->GetBatteryLifetime())));
		}
		else
		{
			if (UIStaticTorch.IsShown())
				UIStaticTorch.Show(false);
			if (UIFlashlightBar.IsShown())
				UIFlashlightBar.Show(false);
		}
	} else {
		if (UIStaticTorch.IsShown())
			UIStaticTorch.Show(false);
		if (UIFlashlightBar.IsShown())
			UIFlashlightBar.Show(false);
	}

	if (m_pActor->UsingTurret())
	{
		if (!UIStaticTurret.IsShown())
			UIStaticTurret.Show(true);
		if (!UITurretBar.IsShown())
			UITurretBar.Show(true);
		UITurretBar.SetProgressPos(100.0f * ((float)m_pActor->GetTurretTemp()) / MAX_FIRE_TEMP);
	}
	else
	{
		UIStaticTurret.Show(false);
		UITurretBar.Show(false);
	}
	UIZoneMap->UpdateRadar			(Device.vCameraPosition);
	float h, p;
	Device.vCameraDirection.getHP	(h,p);
	UIZoneMap->SetHeading			(-h);
	UIActorStateIcons.SetIcons		(m_pActor->GetIconsState());

	UpdatePickUpItem				();
	CUIWindow::Update				();
}

bool CUIMainIngameWnd::OnKeyboardPress(int dik)
{
#pragma todo("wtf")
	if(Level().IR_GetKeyState(DIK_LSHIFT) || Level().IR_GetKeyState(DIK_RSHIFT))
	{
		switch(dik)
		{
		case DIK_NUMPADMINUS:
			UIZoneMap->ZoomOut();
			return true;
			break;
		case DIK_NUMPADPLUS:
			UIZoneMap->ZoomIn();
			return true;
			break;
		}
	}
	else
	{
		switch(dik)
		{
		case DIK_NUMPADMINUS:
			//.HideAll();
			CurrentGameUI()->ShowGameIndicators(false);
			CurrentGameUI()->ShowCrosshair(false);
			return true;
			break;
		case DIK_NUMPADPLUS:
			//.ShowAll();
			CurrentGameUI()->ShowGameIndicators(true);
			CurrentGameUI()->ShowCrosshair(true);
			return true;
			break;
		}
	}
	return false;
}


void CUIMainIngameWnd::RenderQuickInfos()
{
	if (!m_pActor)
		return;

	static CGameObject *pObject			= NULL;
	LPCSTR actor_action					= m_pActor->GetDefaultActionForObject();
	UIStaticQuickHelp.Show				(NULL!=actor_action);

	if(NULL!=actor_action){
		if(stricmp(actor_action,UIStaticQuickHelp.GetText()))
			UIStaticQuickHelp.SetTextST				(actor_action);
	}

	if (pObject!=m_pActor->ObjectWeLookingAt())
	{
		UIStaticQuickHelp.SetTextST				(actor_action?actor_action:" ");
		UIStaticQuickHelp.ResetColorAnimation		();
		pObject	= m_pActor->ObjectWeLookingAt	();
	}
}

void CUIMainIngameWnd::ReceiveNews(GAME_NEWS_DATA* news)
{
	VERIFY(news->texture_name.size());

	CurrentGameUI()->m_pMessagesWnd->AddIconedPdaMessage(*(news->texture_name), news->tex_rect, news->SingleLineText(), news->show_time);
}

void CUIMainIngameWnd::SetWarningIconColor(CUIStatic* s, const u32 cl)
{
	int bOn = (cl>>24);
	bool bIsShown = s->IsShown();

	if(bOn)
		s->SetTextureColor	(cl);

	if(bOn&&!bIsShown){
		m_UIIcons->AddWindow	(s, false);
		s->Show					(true);
	}

	if(!bOn&&bIsShown){
		m_UIIcons->RemoveWindow	(s);
		s->Show					(false);
	}
}

void CUIMainIngameWnd::SetWarningIconColor(EWarningIcons icon, const u32 cl)
{
	bool bMagicFlag = true;

	// ?????? ???? ????????? ??????
	switch(icon)
	{
	case ewiAll:
		bMagicFlag = false;
	case ewiInvincible:
		SetWarningIconColor		(&UIInvincibleIcon, cl);
		if (bMagicFlag) break;
		break;
	default:
		R_ASSERT(!"Unknown warning icon type");
	}
}

void CUIMainIngameWnd::TurnOffWarningIcon(EWarningIcons icon)
{
	SetWarningIconColor(icon, 0x00ffffff);
}


void CUIMainIngameWnd::SetFlashIconState_(EFlashingIcons type, bool enable)
{
	// ???????? ???????? ????????? ??????
	FlashingIcons_it icon = m_FlashingIcons.find(type);
	R_ASSERT2(icon != m_FlashingIcons.end(), "Flashing icon with this type not existed");
	icon->second->Show(enable);
}

#pragma todo("if hud will not include flashing icons, then delete")

void CUIMainIngameWnd::InitFlashingIcons(CUIXml* node)
{
	const char * const flashingIconNodeName = "flashing_icon";
	int staticsCount = node->GetNodesNum("", 0, flashingIconNodeName);

	CUIXmlInit xml_init;
	CUIStatic *pIcon = NULL;
	// ??????????? ?? ???? ????? ? ?????????????? ?? ??? ???????
	for (int i = 0; i < staticsCount; ++i)
	{
		pIcon = new CUIStatic();
		xml_init.InitStatic(*node, flashingIconNodeName, i, pIcon);
		shared_str iconType = node->ReadAttrib(flashingIconNodeName, i, "type", "none");

		// ?????? ?????????? ?????? ? ?? ???
		EFlashingIcons type = efiPdaTask;

		if (iconType == "pda")
			type = efiPdaTask;
		else if (iconType == "mail")
			type = efiMail;
		else if (iconType == "encyclopedia")
			type = efiEncyclopedia;
		else
			R_ASSERT(!"Unknown type of mainingame flashing icon");

		R_ASSERT2(m_FlashingIcons.find(type) == m_FlashingIcons.end(), "Flashing icon with this type already exists");

		CUIStatic* &val	= m_FlashingIcons[type];
		val			= pIcon;

		AttachChild(pIcon);
		pIcon->Show(false);
	}
}

void CUIMainIngameWnd::DestroyFlashingIcons()
{
	for (FlashingIcons_it it = m_FlashingIcons.begin(); it != m_FlashingIcons.end(); ++it)
	{
		DetachChild(it->second);
		xr_delete(it->second);
	}

	m_FlashingIcons.clear();
}

void CUIMainIngameWnd::UpdateFlashingIcons()
{
	for (FlashingIcons_it it = m_FlashingIcons.begin(); it != m_FlashingIcons.end(); ++it)
	{
		it->second->Update();
	}
}

void CUIMainIngameWnd::AnimateContacts(bool b_snd)
{
	UIPdaOnline.ResetXformAnimation	();

	if (CurrentGameUI()->GameIndicatorsShown())
	{
		if (b_snd && g_SingleGameDifficulty != egdMaster)
			HUD_SOUND_ITEM::PlaySound	(m_contactSnd, Fvector().set(0,0,0), 0, true );
	}
}


void CUIMainIngameWnd::SetPickUpItem	(CInventoryItem* PickUpItem)
{
	m_pPickUpItem = PickUpItem;
};

void CUIMainIngameWnd::UpdatePickUpItem	()
{
	if (!m_pPickUpItem || !Level().CurrentViewEntity() || Level().CurrentViewEntity()->CLS_ID != CLSID_OBJECT_ACTOR || !m_pPickUpItem->IsPickUpVisible()) 
	{
		UIPickUpItemIcon.Show(false);
		return;
	};


	shared_str sect_name	= m_pPickUpItem->object().cNameSect();

	//properties used by inventory menu
	int m_iGridWidth	= pSettings->r_u32(sect_name, "inv_grid_width");
	int m_iGridHeight	= pSettings->r_u32(sect_name, "inv_grid_height");

	int m_iXPos			= pSettings->r_u32(sect_name, "inv_grid_x");
	int m_iYPos			= pSettings->r_u32(sect_name, "inv_grid_y");

	float scale_x = m_iPickUpItemIconWidth/
		float(m_iGridWidth*INV_GRID_WIDTH);

	float scale_y = m_iPickUpItemIconHeight/
		float(m_iGridHeight*INV_GRID_HEIGHT);

	scale_x = (scale_x>1) ? 1.0f : scale_x;
	scale_y = (scale_y>1) ? 1.0f : scale_y;

	float scale = scale_x<scale_y?scale_x:scale_y;

	Frect					texture_rect;
	texture_rect.lt.set		(m_iXPos*INV_GRID_WIDTH, m_iYPos*INV_GRID_HEIGHT);
	texture_rect.rb.set		(m_iGridWidth*INV_GRID_WIDTH, m_iGridHeight*INV_GRID_HEIGHT);
	texture_rect.rb.add		(texture_rect.lt);
	UIPickUpItemIcon.GetUIStaticItem().SetTextureRect(texture_rect);
	UIPickUpItemIcon.SetStretchTexture(true);


	UIPickUpItemIcon.SetWidth(m_iGridWidth*INV_GRID_WIDTH*scale*UI().get_current_kx());
	UIPickUpItemIcon.SetHeight(m_iGridHeight*INV_GRID_HEIGHT*scale);

	UIPickUpItemIcon.SetWndPos(Fvector2().set(	m_iPickUpItemIconX+(m_iPickUpItemIconWidth-UIPickUpItemIcon.GetWidth())/2.0f,
												m_iPickUpItemIconY+(m_iPickUpItemIconHeight-UIPickUpItemIcon.GetHeight())/2.0f) );

	UIPickUpItemIcon.SetTextureColor(color_rgba(255,255,255,192));
	UIPickUpItemIcon.Show(true);
};

void CUIMainIngameWnd::UpdateActiveItemInfo()
{
	PIItem	item		= m_pActor->inventory().ActiveItem();
	u32		active_slot	= m_pActor->inventory().GetActiveSlot();
	if(item) 
	{
		xr_string					str_name;
		xr_string					icon_sect_name;
		xr_string					str_count;
		item->GetBriefInfo			(str_name, icon_sect_name, str_count);

		UIWeaponSignAmmo.Show		(true						);
		UIWeaponFiremode.Show		(true						);
		UIWeaponBack.TextItemControl()->SetText		(str_name.c_str			()	);
		UIWeaponFiremode.SetText	(item->GetCurrentFireModeStr());
		UIWeaponSignAmmo.TextItemControl()->SetText	(str_count.c_str		()	);

		SetAmmoIcon					(icon_sect_name.c_str	()	);

		//-------------------
		m_pWeapon = smart_cast<CWeapon*> (item);
		if (active_slot == BOLT_SLOT)
			HandleBolt();		
	}else
	{
		UIWeaponIcon.Show			(false);
		UIWeaponSignAmmo.Show		(false);
		UIWeaponFiremode.Show		(false);
		UIWeaponBack.TextItemControl()->SetText		("");
		m_pWeapon					= NULL;
	}
}

void CUIMainIngameWnd::HandleBolt()
{
	SetAmmoIcon("bolt");
}


void CUIMainIngameWnd::OnConnected()
{
	UIZoneMap->SetupCurrentMap		();
}

void CUIMainIngameWnd::reset_ui()
{
	m_pActor						= NULL;
	m_pWeapon						= NULL;
	m_pGrenade						= NULL;
	m_pItem							= NULL;
	m_pPickUpItem					= NULL;
	UIMotionIcon.ResetVisibility	();
}