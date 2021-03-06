#pragma once

#include "../../../../../Include/xrRender/KinematicsAnimated.h"
#include "../../../actor.h"
#include "../../../../CameraBase.h"
#include "../../../../CustomHUD.h"
#include "../../../../../xrCore/_vector3d_ext.h"

#define TEMPLATE_SPECIALIZATION template <\
	typename _Object\
>

#define CStateBloodsuckerVampireExecuteAbstract CStateBloodsuckerVampireExecute<_Object>

#define VAMPIRE_TIME_HOLD		4000
#define VAMPIRE_HIT_IMPULSE		40.f
#define VAMPIRE_MIN_DIST		0.5f
#define VAMPIRE_MAX_DIST		1.f
extern bool g_bDisableAllInput;

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireExecuteAbstract::initialize()
{
    controlling_value = 1;

	inherited::initialize					();

	object->CControlledActor::install		();

	look_head				();

	m_action				= eActionPrepare;
	time_vampire_started	= 0;

	psHUD_Flags.set(HUD_DRAW, FALSE);
	g_bDisableAllInput = true;

//#pragma todo("SkyLoader: SetWeaponHideState() doesn't work properly in this scheme, need to fix it")
//#if 0
	Actor()->SetWeaponHideState(INV_STATE_BLOCK_ALL, true);
//#endif

	object->stop_invisible_predator	();

	m_effector_activated			= false;
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireExecuteAbstract::execute()
{
	if (!object->CControlledActor::is_turning() && !m_effector_activated) {
		object->ActivateVampireEffector	();
		m_effector_activated			= true;
	}
	
	look_head							();

	switch (m_action) {
		case eActionPrepare:
			execute_vampire_prepare();
			m_action = eActionContinue;
			break;

		case eActionContinue:
			execute_vampire_continue();
			break;

		case eActionFire:
			execute_vampire_hit();
			m_action = eActionWaitTripleEnd;
			break;

		case eActionWaitTripleEnd:
			if (!object->com_man().ta_is_active()) {
				m_action = eActionCompleted; 
			}

		case eActionCompleted:
			controlling_value = 0;
			break;
	}
	object->set_action						(ACT_STAND_IDLE);
	object->dir().face_target	(object->EnemyMan.get_enemy());	
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireExecuteAbstract::show_hud()
{
	psHUD_Flags.set(HUD_DRAW, TRUE);
	Actor()->SetWeaponHideState(INV_STATE_BLOCK_ALL, false);
	g_bDisableAllInput = false;
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireExecuteAbstract::cleanup()
{	
	if ( object->com_man().ta_is_active() )
		object->com_man().ta_deactivate();

	if (object->CControlledActor::is_controlling())
		object->CControlledActor::release		();

	show_hud();
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireExecuteAbstract::finalize()
{
	inherited::finalize();
	cleanup();
	object->start_invisible_predator	();
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireExecuteAbstract::critical_finalize()
{
	inherited::critical_finalize();
	cleanup();
	object->start_invisible_predator	();
}

TEMPLATE_SPECIALIZATION
bool CStateBloodsuckerVampireExecuteAbstract::check_start_conditions()
{
	const CEntityAlive	*enemy = object->EnemyMan.get_enemy();

	if (enemy->CLS_ID != CLSID_OBJECT_ACTOR)							return false;
	
	// ????????? ?????????
	float dist		= object->MeleeChecker.distance_to_enemy(enemy);
	if ((dist > VAMPIRE_MAX_DIST) || (dist < VAMPIRE_MIN_DIST))					return false;

	if (object->CControlledActor::is_controlling())							return false;
	if (current_substate == eStateAttack_RunAttack)							return false;
	if (object->threaten_time() > 0)								return false;

	const CActor *m_actor = smart_cast<const CActor*>(enemy);
	VERIFY(m_actor);
	if (m_actor->input_external_handler_installed())						return false;

	if (controlling_value == 1)									return false;

	// ????????? ??????????? ?? ?????
	if (!object->control().direction().is_face_target(enemy, PI_DIV_6))				return false;

	return true;
}

TEMPLATE_SPECIALIZATION
bool CStateBloodsuckerVampireExecuteAbstract::check_completion()
{
	return (m_action == eActionCompleted);
}

//////////////////////////////////////////////////////////////////////////

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireExecuteAbstract::execute_vampire_prepare()
{
	object->com_man().ta_activate		(object->anim_triple_vampire);
	time_vampire_started				= Device.dwTimeGlobal;
	
	object->sound().play(CAI_Bloodsucker::eVampireGrasp);
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireExecuteAbstract::execute_vampire_continue()
{
	if (object->Position().distance_to(Actor()->Position()) > 2.f) {

	  if (object->com_man().ta_is_active())
		  object->com_man().ta_deactivate();
		m_action = eActionCompleted;
		return;
	}
	
	object->sound().play(CAI_Bloodsucker::eVampireSucking);

	// ????????? ?? ????? ????
	if (time_vampire_started + VAMPIRE_TIME_HOLD < Device.dwTimeGlobal) {
		m_action = eActionFire;
	}
}

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireExecuteAbstract::execute_vampire_hit()
{
	object->com_man().ta_pointbreak				();
	object->sound().play						(CAI_Bloodsucker::eVampireHit);
	object->SatisfyVampire						();
}

//////////////////////////////////////////////////////////////////////////

TEMPLATE_SPECIALIZATION
void CStateBloodsuckerVampireExecuteAbstract::look_head()
{
	IKinematics *pK = smart_cast<IKinematics*>(object->Visual());
	Fmatrix bone_transform;
	bone_transform = pK->LL_GetTransform(pK->LL_BoneID("bip01_head"));	

	Fmatrix global_transform;
	global_transform.mul_43(object->XFORM(),bone_transform);

	object->CControlledActor::look_point	(global_transform.c);
}

#undef TEMPLATE_SPECIALIZATION
#undef CStateBloodsuckerVampireExecuteAbstract

