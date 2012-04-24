/* -------------------------------- Arctic Core ------------------------------
 * Arctic Core - the open source AUTOSAR platform http://arccore.com
 *
 * Copyright (C) 2009  ArcCore AB <contact@arccore.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * -------------------------------- Arctic Core ------------------------------*/

//lint -emacro(904,VALIDATE,VALIDATE_RV,VALIDATE_NO_RV) //904 PC-Lint exception to MISRA 14.7 (validate macros).

#include "Std_Types.h"
#include "EcuM.h"
#include "EcuM_Generated_Types.h"
#include "EcuM_Internals.h"

#if defined(USE_DEM)
#include "Dem.h"
#endif
#if defined(USE_NVM)
#include "NvM.h"
#endif

static uint32 internal_data_run_state_timeout = 0;
#if defined(USE_NVM)
static uint32 internal_data_go_off_one_state_timeout = 0;
static NvM_RequestResultType writeAllResult;
#endif

#ifdef CFG_ECUM_USE_SERVICE_COMPONENT
/** @req EcuM2749 */
static Rte_ModeType_EcuM_Mode currentMode;

void set_current_state(EcuM_StateType state) {
	/* Update the state */
	internal_data.current_state = state;

	Rte_ModeType_EcuM_Mode newMode = currentMode;
	switch( state ) {
	case ECUM_STATE_WAKEUP:
	case ECUM_STATE_WAKEUP_ONE:
	case ECUM_STATE_WAKEUP_VALIDATION:
	case ECUM_STATE_WAKEUP_REACTION:
	case ECUM_STATE_WAKEUP_TWO:
	case ECUM_STATE_SLEEP:
	case ECUM_STATE_SHUTDOWN:
		newMode = RTE_MODE_EcuM_Mode_SLEEP;
		break;
	case ECUM_STATE_GO_SLEEP:
		if( internal_data.shutdown_target == ECUM_STATE_SLEEP ) {
			newMode = RTE_MODE_EcuM_Mode_SLEEP; /** @req EcuM2752 */
		}
		break;
	case ECUM_STATE_GO_OFF_ONE:
	case ECUM_STATE_GO_OFF_TWO:
		newMode = RTE_MODE_EcuM_Mode_SHUTDOWN;
		break;
	case ECUM_STATE_WAKEUP_TTII:
		if( internal_data.shutdown_target == ECUM_STATE_SLEEP ) {
			newMode = RTE_MODE_EcuM_Mode_WAKE_SLEEP; /** @req EcuM2752 */
		}
		break;
	case ECUM_STATE_PREP_SHUTDOWN:
	case ECUM_STATE_APP_POST_RUN: /* Assuming this is same as RUN_III */
		newMode = RTE_MODE_EcuM_Mode_POST_RUN;
		break;
	case ECUM_STATE_APP_RUN: /* Assuming this is same as RUN_II */
		newMode = RTE_MODE_EcuM_Mode_RUN;
		break;
	case ECUM_STATE_STARTUP_TWO:
		newMode = RTE_MODE_EcuM_Mode_STARTUP;
		break;
	default:
		/* Do nothing */
		break;
	}

	if( newMode != currentMode ) {
		currentMode = newMode;
		Rte_Switch_EcuM_CurrentMode_currentMode(currentMode); /** @req EcuM2750 */
	}
}
#endif


void EcuM_enter_run_mode(void){
	set_current_state(ECUM_STATE_APP_RUN);
	EcuM_OnEnterRUN(); /** @req EcuM2308 */
	//TODO: Call ComM_EcuM_RunModeIndication(NetworkHandleType Channel) for all channels that have requested run.
	internal_data_run_state_timeout = internal_data.config->EcuMRunMinimumDuration / ECUM_MAIN_FUNCTION_PERIOD; /** @req EcuM2310 */
}


//--------- Local functions ------------------------------------------------------------------------------------------------

static inline void enter_go_sleep_mode(void){
	set_current_state(ECUM_STATE_GO_SLEEP);
	EcuM_OnGoSleep();
}

static inline void enter_go_off_one_mode(void){
	set_current_state(ECUM_STATE_GO_OFF_ONE);
	EcuM_OnGoOffOne();

#if defined(USE_COMM)
	ComM_DeInit();
#endif

#if defined(USE_NVM)

	// Start NvM_WriteAll and timeout timer
	NvM_WriteAll();

	internal_data_go_off_one_state_timeout = internal_data.config->EcuMNvramWriteAllTimeout / ECUM_MAIN_FUNCTION_PERIOD;
#endif
}


static inline boolean hasRunRequests(void){
	uint32 result = internal_data.run_requests;

#if defined(USE_COMM)
	result |= internal_data.run_comm_requests;
#endif

	return (result != 0);
}

static inline boolean hasPostRunRequests(void){
	return (internal_data.postrun_requests != 0);
}



static inline void in_state_appRun(void){
	if (internal_data_run_state_timeout){
		internal_data_run_state_timeout--;
	}

	if ((!hasRunRequests()) && (internal_data_run_state_timeout == 0)){
		EcuM_OnExitRun();	/** @req EcuM2865 */
		set_current_state(ECUM_STATE_APP_POST_RUN);/** @req EcuM2865 */
	}
}


static inline void in_state_appPostRun(void){
	if (hasRunRequests()){
		set_current_state(ECUM_STATE_APP_RUN);/** @req EcuM2866 */ /** @req EcuM2308 */
		EcuM_OnEnterRUN(); /** @req EcuM2308 */
		//TODO: Call ComM_EcuM_RunModeIndication(NetworkHandleType Channel) for all channels that have requested run.
		internal_data_run_state_timeout = internal_data.config->EcuMRunMinimumDuration / ECUM_MAIN_FUNCTION_PERIOD; /** @req EcuM2310 */

	} else if (!hasPostRunRequests()){
		EcuM_OnExitPostRun(); /** @req EcuM2761 */
		set_current_state(ECUM_STATE_PREP_SHUTDOWN);/** @req EcuM2761 */

		EcuM_OnPrepShutdown();
	} else {
		// TODO: Do something?
	}
}

static inline void in_state_prepShutdown(void){
#if defined(USE_DEM)
	// DEM shutdown
	Dem_Shutdown();
#endif

	// Switch shutdown mode
	switch(internal_data.shutdown_target){
		//If in state Off or Reset go into Go_Off_One:
		case ECUM_STATE_OFF:
		case ECUM_STATE_RESET:
			enter_go_off_one_mode();
			break;
		case ECUM_STATE_SLEEP:
			enter_go_sleep_mode();
			break;
		default:
			//TODO: Report error.
			break;
	}
}

static inline void in_state_goOffOne(void){
#if defined(USE_NVM)
		if (internal_data_go_off_one_state_timeout){
			internal_data_go_off_one_state_timeout--;
		}
		// Wait for the NVM job (NvmWriteAll) to terminate
		NvM_GetErrorStatus(0, &writeAllResult);
		if ((writeAllResult != NVM_REQ_PENDING) || (internal_data_go_off_one_state_timeout == 0)){
			ShutdownOS(E_OK);
		}
#else
		ShutdownOS(E_OK);
#endif
}


//----- MAIN -----------------------------------------------------------------------------------------------------------------
void EcuM_MainFunction(void){
	VALIDATE_NO_RV(internal_data.initiated, ECUM_MAINFUNCTION_ID, ECUM_E_NOT_INITIATED);

	switch(internal_data.current_state){

		case ECUM_STATE_APP_RUN:
			in_state_appRun();
			break;
		case ECUM_STATE_APP_POST_RUN:
			in_state_appPostRun();
			break;
		case ECUM_STATE_PREP_SHUTDOWN:
			in_state_prepShutdown();
			break;
		case ECUM_STATE_GO_OFF_ONE:
			in_state_goOffOne();
			break;
		case ECUM_STATE_GO_SLEEP:
			// TODO: Fill out
			break;
		default:
			//TODO: Report error.
			break;
	}
}
