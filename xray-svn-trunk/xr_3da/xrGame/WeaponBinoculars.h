#pragma once

#include "WeaponCustomPistol.h"
#include "script_export_space.h"

class CUIFrameWindow;
class CUIStatic;
class CBinocularsVision;

class CWeaponBinoculars: public CWeaponCustomPistol
{
private:
	typedef CWeaponCustomPistol inherited;
protected:
	//float			m_fRTZoomFactor; //run-time zoom factor // CWeapon already has this declaration
	bool			m_bVision;
public:
					CWeaponBinoculars	(); 
	virtual			~CWeaponBinoculars	();

	void			Load				(LPCSTR section);

	virtual void	OnZoomIn			();
	virtual void	OnZoomOut			();
	virtual	void	ZoomInc				();
	virtual	void	ZoomDec				();
	virtual void	net_Destroy			();
	virtual BOOL	net_Spawn			(CSE_Abstract* DC);

	//tatarinrafa: Uncoment when needed
//	virtual void	save				(NET_Packet &output_packet);
//	virtual void	load				(IReader &input_packet);

	virtual bool	Action				(u16 cmd, u32 flags);
	virtual void	UpdateCL			();
	virtual bool	use_crosshair		()	const {return false;}
	virtual void	render_item_ui		();
	virtual bool	render_item_ui_query();
	virtual void	GetBriefInfo		(xr_string& str_name, xr_string& icon_sect_name, xr_string& str_count);
	virtual void	net_Relcase			(CObject *object);

	virtual LPCSTR	GetCurrentFireModeStr	() {return " ";};
protected:
	CBinocularsVision*					m_binoc_vision;

	DECLARE_SCRIPT_REGISTER_FUNCTION
};
add_to_type_list(CWeaponBinoculars)
#undef script_type_list
#define script_type_list save_type_list(CWeaponBinoculars)
