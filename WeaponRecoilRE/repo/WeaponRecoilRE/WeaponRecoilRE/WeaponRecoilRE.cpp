#include "plugin.h"
//>
#include "include/IniReader-master/IniReader.h"
#include "extensions/ScriptCommands.h"
#include "include/SA-MixSets-master/MixSets/TestCheat.h"
#include "CHud.h"
#include "CGeneral.h"
#include "CTimer.h"
//<

using namespace plugin;
//>
using namespace std;

const string MODNAME = "WeaponRecoilRE";

const float PI6 = 3.141592f;

const string KEYMODE = "iRecoilMode ";
const string KEYMULTIPLIER = "fRecoilMultiplier ";
const string KEYRETURN = "fRecoilReturn ";
const string KEYGLOBAL = "Global";
const float INITIALMULTIPLIER = PI6 * 0.005f;
const float INITIALRETURN = PI6 * 0.0005f;
const int DEFAULTMODE = 0;
const float DEFAULTMULTIPLIER = 1.0f;
const float DEFAULTRETURN = 1.0f;

float tapsmultiplier;

float savedmultiplier;

void resetRecoil() {
	tapsmultiplier = 0.0f;

	savedmultiplier = 0.0f;
}

bool flaloaded = false;

typedef int(_cdecl *parentprocedure)(int);
parentprocedure parentID;

bool iniread = false;

CIniReader inifile;

bool patchautoaim = false;

int weaponsaved = -1;

int getParent(int weapontype) {
	int weaponparent = weapontype;

	if (parentID) {
		int parentweapon = parentID(weapontype);
		if (parentweapon != -1) {
			weaponparent = parentweapon;
		}
	}

	return weaponparent;
}

void getRecoil(string datatype, int dataid, int &recoilmode, float &recoilmultiplier, float &recoilreturn) {
	string datastring = datatype + to_string(dataid);

	recoilmode = inifile.ReadInteger(MODNAME, KEYMODE + datastring, recoilmode);
	recoilmultiplier *= inifile.ReadFloat(MODNAME, KEYMULTIPLIER + datastring, DEFAULTMULTIPLIER);
	recoilreturn *= inifile.ReadFloat(MODNAME, KEYRETURN + datastring, DEFAULTRETURN);
}

const int RECOILX = 0xB6F258;
const int RECOILY = 0xB6F248;
void doRecoil(float recoilangle, float recoilmultiplier) {
	recoilangle = (recoilangle * PI6) / 180.0f;

	patch::SetFloat(RECOILX, patch::GetFloat(RECOILX) + cosf(recoilangle) * recoilmultiplier);
	patch::SetFloat(RECOILY, patch::GetFloat(RECOILY) + sinf(recoilangle) * recoilmultiplier);
}

float savedangle;
//<

class WeaponRecoilRE {
public:
	WeaponRecoilRE() {
		// Initialise your plugin here

//>
		resetRecoil();

		Events::processScriptsEvent += [] {
			if (!flaloaded) {
				HMODULE flamodule = GetModuleHandle("$fastman92limitAdjuster.asi");
				if (flamodule) {
					parentID = (parentprocedure)GetProcAddress(flamodule, "GetWeaponHighestParentType");
				}

				flaloaded = true;
			}

			if (Command<Commands::IS_PLAYER_PLAYING>(0)) {
				CPed *playerped;
				Command<Commands::GET_PLAYER_CHAR>(0, &playerped);

				if (TestCheat("RSWRR")) {
					iniread = false;

					CHud::SetHelpMessage((MODNAME + " Reloaded.").c_str(), false, false, false);
				}

				if (!iniread) {
					(&inifile)->~CIniReader();
					new(&inifile) CIniReader(MODNAME + ".ini");

					resetRecoil();

					iniread = true;
				}

				if (!patchautoaim) {
					if (!inifile.ReadBoolean(MODNAME, "bEnableAutoaim", false)) {
						patch::SetChar(0x686905, 0xB0);
						patch::SetInt(0x686906, 0x90909001);
					}

					patchautoaim = true;
				}

				int weapontype;
				Command<Commands::GET_CURRENT_CHAR_WEAPON>(playerped, &weapontype);

				if (weaponsaved != weapontype) {
					resetRecoil();

					weaponsaved = weapontype;
				}

				int weaponparent = getParent(weapontype);

				int weaponskill = playerped->GetWeaponSkill();
				CWeaponInfo *weaponinfo = CWeaponInfo::GetWeaponInfo(eWeaponType(weapontype), weaponskill);
				float weaponaccuracy = weaponinfo->m_fAccuracy;
				if (weaponaccuracy < inifile.ReadFloat(MODNAME, "fAccuracyThreshold", 10.0f)) {
					int weaponanimgroup = weaponinfo->m_dwAnimGroup;

					int recoilmode = inifile.ReadInteger(MODNAME, KEYMODE + KEYGLOBAL, DEFAULTMODE);
					float recoilmultiplier = INITIALMULTIPLIER * inifile.ReadFloat(MODNAME, KEYMULTIPLIER + KEYGLOBAL, DEFAULTMULTIPLIER);
					float recoilreturn = INITIALRETURN * inifile.ReadFloat(MODNAME, KEYRETURN + KEYGLOBAL, DEFAULTRETURN);

					recoilmultiplier *= 1.0f / weaponaccuracy;
					if (Command<Commands::IS_CHAR_STOPPED>(playerped)) {
						recoilmultiplier *= inifile.ReadFloat(MODNAME, KEYMULTIPLIER + "Stopped", 0.75f);
					}
					if (Command<Commands::IS_CHAR_DUCKING>(playerped)) {
						recoilmultiplier *= inifile.ReadFloat(MODNAME, KEYMULTIPLIER + "Ducking", 0.75f);
					}
					int cameramode = TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode;
					if (cameramode == eCamMode::MODE_AIMWEAPON_FROMCAR) {
						recoilmultiplier *= inifile.ReadFloat(MODNAME, KEYMULTIPLIER + "Driveby", 1.0f);
					}
					if (weaponinfo->m_nFlags.bCanAim == 1) {
						recoilmultiplier *= inifile.ReadFloat(MODNAME, KEYMULTIPLIER + "Singlehanded", 1.25f);
					}
					if (weaponinfo->m_nFlags.bTwinPistol == 1) {
						recoilmultiplier *= inifile.ReadFloat(MODNAME, KEYMULTIPLIER + "Dual", 1.5f);
					}

					getRecoil("Type", weapontype, recoilmode, recoilmultiplier, recoilreturn);
					getRecoil("Parent", weaponparent, recoilmode, recoilmultiplier, recoilreturn);
					getRecoil("Animgroup", weaponanimgroup, recoilmode, recoilmultiplier, recoilreturn);

					if (recoilmode != -1 && recoilmultiplier > 0.0f) {
						bool ignoreaim = inifile.ReadBoolean(MODNAME, "bIgnoreAim", false);
						if (ignoreaim
							|| cameramode == eCamMode::MODE_AIMING
							|| cameramode == eCamMode::MODE_AIMWEAPON
							|| cameramode == eCamMode::MODE_AIMWEAPON_FROMCAR
							|| cameramode == eCamMode::MODE_AIMWEAPON_ATTACHED

							|| cameramode == eCamMode::MODE_SNIPER
							|| cameramode == eCamMode::MODE_ROCKETLAUNCHER
							|| cameramode == eCamMode::MODE_1STPERSON
							|| cameramode == eCamMode::MODE_M16_1STPERSON
							|| cameramode == eCamMode::MODE_SNIPER_RUNABOUT
							|| cameramode == eCamMode::MODE_ROCKETLAUNCHER_RUNABOUT
							|| cameramode == eCamMode::MODE_1STPERSON_RUNABOUT
							|| cameramode == eCamMode::MODE_M16_1STPERSON_RUNABOUT
							|| cameramode == eCamMode::MODE_HELICANNON_1STPERSON
							|| cameramode == eCamMode::MODE_CAMERA
							|| cameramode == eCamMode::MODE_ROCKETLAUNCHER_HS
							|| cameramode == eCamMode::MODE_ROCKETLAUNCHER_RUNABOUT_HS
							) {
							if (Command<Commands::IS_CHAR_SHOOTING>(playerped)) {
								int weaponfiretype = weaponinfo->m_nWeaponFire;

								bool weapondefault;
								switch (weaponfiretype) {
								case 1:
								case 2: {
									weapondefault = true;
									break;
								}
								default: {
									weapondefault = false;
								}
								}
								if (inifile.ReadBoolean(MODNAME, "bFiretypeEnabled " + to_string(weaponfiretype), weapondefault)) {
									float recoilangle;
									switch (recoilmode) {
									case 1: {
										recoilangle = 90.0f;
										break;
									}
									case 2: {
										recoilangle = CGeneral::GetRandomNumberInRange(0.0f, 180.0f);
										break;
									}
									case 3: {
										tapsmultiplier = clamp(tapsmultiplier, 0.0f, 1.0f);
										float tapslow = 90.0f - (90.0f * tapsmultiplier);
										float tapshigh = 90.0f + (90.0f * tapsmultiplier);
										tapsmultiplier += recoilmultiplier;
										recoilangle = CGeneral::GetRandomNumberInRange(tapslow, tapshigh);
										break;
									}
									default: {
										recoilangle = CGeneral::GetRandomNumberInRange(0.0f, 360.0f);
									}
									}

									if (!inifile.ReadBoolean(MODNAME, "bDisableReturn", false)) {
										if (recoilangle == (savedangle - 180.0f) && !inifile.ReadBoolean(MODNAME, "bDisableAdditive", false)) {
											savedmultiplier += recoilmultiplier;
										}
										else {
											savedmultiplier = recoilmultiplier;
										}
										savedangle = recoilangle + 180.0f;
									}

									doRecoil(recoilangle, recoilmultiplier);
								}
							}
							else {
								float timestep = CTimer::ms_fTimeStep;

								if (savedmultiplier > 0.0f) {
									float recoilsubtract = recoilreturn * (1.0f / (recoilmultiplier * 100.0f)) * timestep;
									if (recoilsubtract > savedmultiplier) {
										recoilsubtract = savedmultiplier;
									}
									savedmultiplier -= recoilsubtract;
									doRecoil(savedangle, recoilsubtract);

									if (!ignoreaim && inifile.ReadBoolean(MODNAME, "bForceAim", false)) {
										patch::SetUShort(0xB73464, 255);
									}
								}

								tapsmultiplier -= recoilmultiplier * 0.1f * timestep;
							}
						}
						else {
							resetRecoil();
						}
					}
				}
			}
		};

		Events::drawHudEvent.before += [] {
			if (inifile.ReadBoolean(MODNAME, "bDisableSpread", false)) {
				patch::SetFloat(0xB7CDC8, 0.0f);
			}
		};
		//<
	}
	//>
	//<
} weaponRecoilRE;

//>
//<