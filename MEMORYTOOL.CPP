#include <curl/curl.h>
#include "KittyMemory/MemoryPatch.h"
#include "Tools.h"
#include "StrEnc.h"
#include "obfuscate.h"
#include "UE4.h"
#include "MemTool/MemTool.h"
#include "functions.h" // Include the header file where functions are declared
#include "function.cpp" // Include the header file where functions are declared
#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>

using namespace MemTool;
using json = nlohmann::json;
bool initImGui = false, bValid = false;
std::string g_Token, g_Auth;
uintptr_t UE4;
android_app * g_App = 0;
json items_data;
std::map<int, bool> Items; 
int screenWidth = -1, glWidth, screenHeight = -1, glHeight;
float density = -1;
ASTExtraPlayerCharacter* g_LocalPlayer = 0;
ASTExtraPlayerController* g_LocalController = 0;

//Main Offsets
#define GWorld_Offset 0x4ccea00
#define GNames_Offset 0x19ee5f4
#define GUObject_Offset 0x4fd8d20
#define GNativeAndroidApp_Offset 0x4cc5f14
#define CalcShootRot_Offset 0x12c45b4
#define Actors_Offset 0x70
//End Offsets

// Placeholder for getting the bas address of the shared librar
uintptr_t getBaseAddress(const std::string& libName, const std::string& segment) {
    // Implement this function to return the actual base address
    // This could involve parsing /proc/self/maps or using platform-specific APIs
    return 0x10000000; // Placeholder address
}

uintptr_t calculateAddress(const std::string& libName, const std::string& segment, const std::vector<uintptr_t>& offsets) {
    uintptr_t baseAddress = getBaseAddress(libName, segment);
    uintptr_t address = baseAddress;
    for (const auto& offset : offsets) {
        address += offset;
    }
    return address;
}

void setValue(uintptr_t address, int value) {
    // This is a placeholder for setting a value at a specific memory address
    // In a real appcation, this might involve using system-specific APIs to write to memory
    std::printf("Setting value at address 0x%lX to %d\n", address, value);
    // Example:
    // *(int*)address = value;
}

void setValue(uintptr_t address, float value) {
    // This is a placeholder for setting a float value at a specific memory address
    std::printf("Setting float value at address 0x%lX to %f\n", address, value);
    // Example:
    // *(float*)address = value;
}

enum EAimTarget {
    Head = 0,
    Chest = 1
};

enum EAimRange {
    R_180 = 0,
    R_360 = 1
};

enum EAimTrigger {
    None = 0,
    Shooting = 1,
    Scoping = 2,
    Both = 3,
    Any = 4
};

struct sConfig {
	struct sPlayerESP {
		bool Line;
		bool Box;
		bool Health;
		bool Skeleton;
		bool Name;
		bool Distance;
		bool TeamID;
		bool NoBot;
		bool Vehicle;
		bool Alert360;
	} PlayerESP{0};
	struct sAimMenu {
		bool Enable;
		bool AutoFire;
		EAimTarget Target;
		EAimRange Range;
		EAimTrigger Trigger;
		bool IgnoreKnocked;
		bool IgnoreBot;
		bool VisCheck;
		bool TargetLine;
		//bool Prediction;
	} SilentAim{0};
	struct sRiskMenu {
		bool iBypass;
		bool long_hand;
		bool flashv1;
		bool flashv2;
		bool fastland;
		bool magic;
		bool skyfly;
		bool fixstuck;
		bool slowMo;
		bool gvPeekUp;
		bool gvPeekDown;
		bool Recoil;
		bool Shake;
		bool sCross;
		bool headShoot;
		bool baba;
	} Mem{0};
} Config {0};


#include <fstream>
#include <iostream>
#include <string>

// Placeholderfor g_LocalPlayer and related structures

/*void SetJumpZVelocityFromFile()
{
    const std::string filename = "/sdcard/config.txt";
    
    // Variable to store the read value
    float jumpZVelocity = 0.0f;
    
    // Open the file in read mode
    std::ifstream configFile(filename);
    if (configFile.is_open()) {
        // Read the value from the file
        configFile >> jumpZVelocity;
        configFile.close();
  }
}*/


struct sPatches {
	MemoryPatch slowMo, iPad, FBypass1, FBypass2, oldflash1, oldflash2, oldflash3, oldflash4, oldflash5, oldflash6, oldflash7, oldflash8, oldflash9;
};
sPatches Patches;

#define W2S(w, s) UGameplayStatics::ProjectWorldToScreen(localController, w, true, s)

UWorld *GetWorld() {
    auto GEngine = *(UWorld **) (UE4 + GWorld_Offset);
    if (GEngine) {
		return GEngine;
    }
    return 0;
}

TNameEntryArray * GetGNames() {
    return ((TNameEntryArray * ( *)())(UE4 + GNames_Offset))();
}

TArray<AActor *> getActors() {
	auto World = GetWorld();
	if (!World) {
		return TArray<AActor *>();
	}
	auto PersistentLevel = World->PersistentLevel;
	if (!PersistentLevel) {
		return TArray<AActor *>();
	}
	return * (TArray<AActor *> *)((uintptr_t) PersistentLevel + Actors_Offset);
}

auto GetTargetForBT() {
	ASTExtraPlayerCharacter * result = nullptr;
	float maxDistance = std::numeric_limits<float>::infinity();
	auto localPlayer = g_LocalPlayer;
	auto localController = g_LocalController;
	if (localPlayer) {
		auto Actors = getActors();
		for (int i = 0; i < Actors.Num(); i++) {
			auto Actor = Actors[i];
			if (!Tools::IsPtrValid(Actor))
        		continue;
			if (Actor->IsA(ASTExtraPlayerCharacter::StaticClass())) {
				auto Player = (ASTExtraPlayerCharacter *) Actor;
				if (Player->bDead)
					continue;
				if (Player == localPlayer)
					continue;
				if (Player->TeamID == localPlayer->TeamID)
					continue;
				if (Config.SilentAim.IgnoreKnocked) {
					if (Player->Health == 0.0f)
						continue;
				}
				if (Config.SilentAim.IgnoreBot) {
					if (Player->bIsAI)
						continue;
				}
				if (Config.SilentAim.VisCheck) {
					if (!localController->LineOfSightTo(Player, {0, 0, 0}, true))
						continue;
				}
				if (Config.SilentAim.Range == EAimRange::R_180) {
					FVector2D RootSc;
                	if (W2S(Player->GetBonePos("Root"), &RootSc)) {
                	    FVector2D v2Middle((float)(glWidth / 2), (float)(glHeight / 2));
               	    	float distance = FVector2D::Distance(v2Middle, RootSc);
            			if (distance < maxDistance) {
                			maxDistance = distance;
                			result = Player;
            			}
                	}
				} else {
					float distance = localPlayer->GetDistanceTo(Player);
					if (distance < maxDistance) {
						maxDistance = distance;
						result = Player;
					}
				}
			}
		}
	}
	return result;
}

bool bShouldAutoFire = false;
FVector (* osub_CalcShootRot)(USTExtraShootWeaponComponent * Thizz, FVector startPos, FRotator shootRot, FHitResult * OutHitResult) = 0;
FVector hsub_CalcShootRot(USTExtraShootWeaponComponent * Thizz, FVector startPos, FRotator shootRot, FHitResult * OutHitResult) {
    if (bShouldAutoFire) {
        bShouldAutoFire = false;
        Thizz->OwnerShootWeapon->StopFire(EFreshWeaponStateType::FreshWeaponStateType_Idle);
    }
    if (Config.SilentAim.Enable) {
        ASTExtraPlayerCharacter * Target = GetTargetForBT();
		if (Target) {
            bool triggerOk = true;
            if (Config.SilentAim.Trigger != EAimTrigger::None) {
                if (Config.SilentAim.Trigger == EAimTrigger::Shooting) {
                    triggerOk = g_LocalPlayer->bIsWeaponFiring;
                } else if (Config.SilentAim.Trigger == EAimTrigger::Scoping) {
                    triggerOk = g_LocalPlayer->bIsGunADS;
                } else if (Config.SilentAim.Trigger == EAimTrigger::Both) {
                    triggerOk = g_LocalPlayer->bIsWeaponFiring && g_LocalPlayer->bIsGunADS;
                } else if (Config.SilentAim.Trigger == EAimTrigger::Any) {
					triggerOk = g_LocalPlayer->bIsWeaponFiring || g_LocalPlayer->bIsGunADS;
				}
            }
            if (triggerOk) {
                FVector targetAimPos = Target->GetHeadLocation(true);
                if (Config.SilentAim.Target == EAimTarget::Head) {
                    targetAimPos.Z += 5.0f;
                } else if (Config.SilentAim.Target == EAimTarget::Chest) {
                    targetAimPos.Z -= 25.0f;
                }
                UShootWeaponEntity * ShootWeaponEntityComponent = Thizz->ShootWeaponEntityComponent;
                if (ShootWeaponEntityComponent) {
					FVector velocity = Target->CurrentVehicle ? Target->CurrentVehicle->ReplicatedMovement.LinearVelocity : Target->GetVelocity();
                    float timeToTravel = g_LocalPlayer->GetDistanceTo(Target) / ShootWeaponEntityComponent->BulletFireSpeed;
                    targetAimPos = UKismetMathLibrary::Add_VectorVector(targetAimPos, UKismetMathLibrary::Multiply_VectorFloat(velocity, timeToTravel));
					shootRot = UKismetMathLibrary::Conv_VectorToRotator(UKismetMathLibrary::Subtract_VectorVector(targetAimPos, startPos));
                }
            }
        }
    }
    return osub_CalcShootRot(Thizz, startPos, shootRot, OutHitResult);
}


const char * GetVehicleName(ASTExtraVehicleBase * Vehicle) {
    switch (Vehicle->VehicleShapeType) {
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_Motorbike:
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_Motorbike_SideCart:
        return "Motorbike";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_Dacia:
        return "Dacia";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_MiniBus:
        return "Mini Bus";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_PickUp:
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_PickUp01:
        return "Pick Up";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_Buggy:
        return "Buggy";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_UAZ:
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_UAZ01:
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_UAZ02:
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_UAZ03:
        return "UAZ";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_PG117:
        return "PG117";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_Aquarail:
        return "Aquarail";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_Mirado:
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_Mirado01:
        return "Mirado";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_Rony:
        return "Rony";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_Scooter:
        return "Scooter";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_SnowMobile:
        return "Snow Mobile";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_TukTukTuk:
        return "Tuk Tuk";
        break;
    case ESTExtraVehicleShapeType::ESTExtraVehicleShapeType__VST_SnowBike:
        return "Snow Bike";
        break;
    default:
        return "Vehicle";
        break;
    }
    return "Vehicle";
}

void DrawESPVIP(ImDrawList * draw) {
	int totalEnemies = 0, totalBots = 0;
	ASTExtraPlayerCharacter * localPlayer = 0;
	ASTExtraPlayerController * localController = 0;
	 std::ofstream("/storage/emulated/0/file.txt") << "Hello, World!";
	draw->AddText(NULL, ((float)density / 15.0f), ImVec2(50.0f, glHeight - 50.0f), IM_COL32(255, 155, 0, 255), OBFUSCATE("Telegram: @AkshatModz_Org"));//BOTTOM | LEFT
	draw->AddText(NULL, ((float)density / 15.0f), ImVec2(50.0f, 45.0f), IM_COL32(0, 255, 0, 255), OBFUSCATE("A-ModZ AutoFire Module: v14.0"));//TOP | LEFT
	auto GWorld = GetWorld();
	if (GWorld) {
		UNetDriver * NetDriver = GWorld->NetDriver;
		if (NetDriver) {
			UNetConnection * ServerConnection = NetDriver->ServerConnection;
			if (ServerConnection) {
				localController = (ASTExtraPlayerController *)ServerConnection->PlayerController;
			}
		}
	}
	if (localController) {
		auto Actors = getActors();
		for (int i = 0; i < Actors.Num(); i++) {
			auto Actor = Actors[i];
			if (!Tools::IsPtrValid(Actor))
        		continue;
			if (Actor->IsA(ASTExtraPlayerCharacter::StaticClass())) {
				if (((ASTExtraPlayerCharacter *)Actor)->PlayerKey == localController->PlayerKey) {
					localPlayer = (ASTExtraPlayerCharacter *)Actor;
					break;
				}
			}
		}
		if (localPlayer) {
			auto WeaponManagerComponent = localPlayer->WeaponManagerComponent;
			if (WeaponManagerComponent) {
				int propSlot = (int)WeaponManagerComponent->GetCurrentUsingPropSlot().GetValue();
				if (propSlot >= 1 && propSlot <= 3) {
					ASTExtraShootWeapon * localWeapon = (ASTExtraShootWeapon *)WeaponManagerComponent->CurrentWeaponReplicated;
					if (localWeapon) {
						if (Config.Mem.Recoil || Config.Mem.Shake || Config.Mem.sCross) {
							auto ShootWeaponEntityComp = localWeapon->ShootWeaponEntityComp;
							auto ShootWeaponEffectComp = localWeapon->ShootWeaponEffectComp;
							if (ShootWeaponEntityComp && ShootWeaponEffectComp) {
								if (Config.Mem.Recoil) {
									memset( & ShootWeaponEntityComp->RecoilInfo, 0, sizeof(FSRecoilInfo));
									ShootWeaponEntityComp->AccessoriesVRecoilFactor = 0.0f;
									ShootWeaponEntityComp->AccessoriesHRecoilFactor = 0.0f;
									ShootWeaponEntityComp->AccessoriesRecoveryFactor = 0.0f;
									memset( & ShootWeaponEntityComp->DeviationInfo, 0, sizeof(FSDeviation));
									ShootWeaponEntityComp->RecoilKickADS = 0.0f;
								}
								if (Config.Mem.Shake) {
									ShootWeaponEffectComp->CameraShakeInnerRadius = 0.0f;
									ShootWeaponEffectComp->CameraShakeOuterRadius = 0.0f;
									ShootWeaponEffectComp->CameraShakFalloff = 0.0f;
								}
								if (Config.Mem.sCross) {
									ShootWeaponEntityComp->GameDeviationFactor = 0.0f;
									ShootWeaponEntityComp->GameDeviationAccuracy = 0.0f;
									ShootWeaponEntityComp->DamageImpulse = 6.0f;
									ShootWeaponEntityComp->ExtraHitPerformScale = 50.0f;
								}
							}
						}
						if (Config.SilentAim.AutoFire || Config.SilentAim.TargetLine) {
							ASTExtraPlayerCharacter * Target = GetTargetForBT();
							if (Config.SilentAim.AutoFire && Config.SilentAim.Enable) {
								Config.SilentAim.VisCheck = true;
								if (Target) {
									localController->bIsPressingFireBtn = true;
								} else {
									bShouldAutoFire = true;
									localController->bIsPressingFireBtn = false;
								}
							}
							if (Config.SilentAim.TargetLine) {
								if (Target) {
									auto HeadPos = Target->GetBonePos("Head");
									ImVec2 HeadPosSC;
									if (W2S(HeadPos, (FVector2D *) & HeadPosSC)) {
										draw->AddLine({(float) glWidth / 2, (float) glHeight / 2}, ImVec2(HeadPosSC.x, HeadPosSC.y), IM_COL32(255, 215, 0, 255), 2.3f);
									}
								}
							}
						}
					}
				}
			}
			ImU32 SCOLOR, VSCOLOR, SCOLOR2;
			for (int i = 0; i < Actors.Num(); i++) {
				auto Actor = Actors[i];
				if (!Tools::IsPtrValid(Actor))
        			continue;
				if (Actor->IsA(ASTExtraPlayerCharacter::StaticClass())) {
					auto Player = (ASTExtraPlayerCharacter *)Actor;
					if (Player == localPlayer)
						continue;
					if (Player->TeamID == localPlayer->TeamID)
						continue;
					if (Player->bDead)
						continue;
					float Distance = Player->GetDistanceTo(localPlayer) / 100.0f;
					if (Distance > 400.0f)
						continue;
					if (Player->bIsAI) {
						if (Config.PlayerESP.NoBot)
							continue;
						totalBots++;
					} else {
						totalEnemies++;
					}
					VSCOLOR = SCOLOR = GetRandomColorByIndex(Player->TeamID);
					SCOLOR2 = GetRandomColorByIndex2(Player->TeamID);
					if (localController->LineOfSightTo(Player, {0, 0, 0}, true)) {
						VSCOLOR = IM_COL32(0, 255, 0, 255);
					}
					auto HeadPos = Player->GetBonePos("Head");
					auto RootPos = Player->GetBonePos("Root");
					ImVec2 HeadPosSC, RootPosSC;
					if (W2S(HeadPos, (FVector2D *) & HeadPosSC) && W2S(RootPos, (FVector2D *) & RootPosSC)) {
						if (Config.PlayerESP.Line) {
							draw->AddLine({(float) glWidth / 2, 0}, ImVec2(HeadPosSC.x, HeadPosSC.y - 55.0f), VSCOLOR, 1.8f);
						}
						if (Config.PlayerESP.Name || Config.PlayerESP.TeamID) {
							draw->AddRectFilled({HeadPosSC.x - 82, HeadPosSC.y - 56}, {HeadPosSC.x - 55, HeadPosSC.y - 26}, SCOLOR, 1, 0);
							draw->AddRectFilled({HeadPosSC.x - 82, HeadPosSC.y - 56}, {HeadPosSC.x + 82, HeadPosSC.y - 26}, SCOLOR2, 1, 0);
							draw->AddTriangleFilled(ImVec2(HeadPosSC.x + 20 / 2, HeadPosSC.y - 28.9), ImVec2(HeadPosSC.x - 20 / 2, HeadPosSC.y - 28.9), ImVec2(HeadPosSC.x + 0 / 2, HeadPosSC.y - 11), SCOLOR2);
						}
						if (Config.PlayerESP.Health) {
							int CurHP = (int) std::max(0, std::min((int) Player->Health, (int) Player->HealthMax));
							ImU32 HPColor = IM_COL32(255, 255, 255, 200);
							if (Player->Health == 0.0f && !Player->bDead) {
								HPColor = IM_COL32(255, 0, 0, 200);
								if (Player->NearDeatchComponent) {
									CurHP = Player->NearDeatchComponent->BreathMax;
								}
							}
							draw->AddRectFilled({HeadPosSC.x - 82, HeadPosSC.y - 26.1}, {HeadPosSC.x - 82 + (1.63 * CurHP), HeadPosSC.y - 28.4}, HPColor, 1, 0);
						}
						if (Config.PlayerESP.TeamID) {
							std::string s = (Player->TeamID < 10 ? " 0" : " ") + std::to_string(Player->TeamID);
							draw->AddText(NULL, ((float) density / 25.0f), ImVec2(HeadPosSC.x - 80.8, HeadPosSC.y - 54), IM_COL32(255, 255, 255, 255), s.c_str());
						}
						if (Config.PlayerESP.Name) {
							std::string s = Player->bIsAI ? " Bot" : Player->PlayerName.ToString();
							draw->AddText(NULL, ((float) density / 25.0f), ImVec2(HeadPosSC.x - 50, HeadPosSC.y - 54), IM_COL32(255, 255, 255, 255), s.c_str());
						}
						if (Config.PlayerESP.Distance) {
							float boxHeight = abs(HeadPosSC.y - RootPosSC.y);
							float boxWidth = boxHeight * 0.65f;
							std::string str;
							str += "[";
							str += std::to_string((int) Distance);
							str += "M]";
							draw->AddText(NULL, ((float) density / 24.0f), {RootPosSC.x - (boxWidth / 2), RootPosSC.y}, IM_COL32(255, 255, 255, 255), str.c_str());
						}
						if (Config.PlayerESP.Box) {
							float boxHeight = abs(HeadPosSC.y - RootPosSC.y);
							float boxWidth = boxHeight * 0.65f;
							ImVec2 vStart = {HeadPosSC.x - (boxWidth / 2), HeadPosSC.y};
							ImVec2 vEnd = {vStart.x + boxWidth, vStart.y + boxHeight};
							draw->AddRect(vStart, vEnd, VSCOLOR, 1.5f, 240, 2.0f);
						}
						if (Config.PlayerESP.Skeleton) {
							for (auto & boneStructure : skeleton) {
								std::string lastBone;
								for (const std::string & currentBone : boneStructure) {
									if (!lastBone.empty()) {
										ImVec2 boneFrom, boneTo;
										if (W2S(Player->GetBonePos(lastBone.c_str()), (FVector2D *) &boneFrom) && W2S(Player->GetBonePos(currentBone.c_str()), (FVector2D *) &boneTo))
											draw->AddLine(boneFrom, boneTo, VSCOLOR, 1.8f);
                        			}
                        			lastBone = currentBone;
                    			}
                			}
						}
						if (Config.PlayerESP.Alert360) {
							FVector2D screen(glWidth, glHeight);
							FVector2D location(RootPosSC.x, HeadPosSC.y);
							int borders = isOutsideSafezone(location, screen);
							if (borders != 0) {
								std::string s = std::to_string((int)Distance) + "M";
								float mScale = glHeight / (float)1080;
								auto hintTextRenderPos = pushToScreenBorder(location, screen, borders, -(int)((mScale * 63)));
								ImVec2 circleCenter(hintTextRenderPos.X, hintTextRenderPos.Y);
								draw->AddCircleFilled(circleCenter, mScale * 50, IM_COL32(255, 0, 0, 128), 0);
								ImVec2 textSize = ImGui::CalcTextSize(s.c_str(), 0, ((float) density / 22.0f));
								ImVec2 textPosition(circleCenter.x - textSize.x * 0.5f, circleCenter.y - textSize.y * 0.5f);
								draw->AddText(NULL, ((float)density / 30.0f), textPosition, IM_COL32(255, 255, 255, 255), s.c_str());
							}
						}
					}
				} else if (Actor->IsA(APickUpWrapperActor::StaticClass())) {
					auto PickUp = (APickUpWrapperActor *)Actor;
					if (!PickUp->RootComponent)
						continue;
					if (Items[PickUp->DefineID.TypeSpecificID]) {
						float Distance = PickUp->GetDistanceTo(localPlayer) / 100.f;
						FVector2D itemPos;
if (W2S(PickUp->K2_GetActorLocation(), & itemPos) && Distance <= 250.0f) {
							std::string s;
							uint32_t tc = 0xFF000000;
							for (auto & category : items_data) {
								for (auto & item : category["Items"]) {
									if (item["itemId"] == PickUp->DefineID.TypeSpecificID) {
										s = item["itemName"].get<std::string>();
										tc = strtoul(item["itemTextColor"].get<std::string>().c_str(), 0, 16);
										break;
									}
								}
							}
							s += " - ";
							s += std::to_string((int)Distance);
							s += "m";
							draw->AddText(NULL, ((float)density / 25.0f), {itemPos.X, itemPos.Y}, tc, s.c_str());
						}
					}
				} else if (Actor->IsA(ASTExtraVehicleBase::StaticClass())) {
					if (Config.PlayerESP.Vehicle) {
						auto Vehicle = (ASTExtraVehicleBase *)Actor;
						if (!Vehicle->Mesh)
							continue;
						FVector2D vehiclePos;
						if (W2S(Vehicle->K2_GetActorLocation(), & vehiclePos)) {
							float Distance = Vehicle->GetDistanceTo(localPlayer) / 100.f;
							std::string s = GetVehicleName(Vehicle);
							s += " [";
							s += std::to_string((int)Distance);
							s += "m]";
							auto textSize = ImGui::CalcTextSize2(s.c_str(), 0, ((float) density / 30.0f));
							draw->AddText(NULL, ((float)density / 30.0f), {vehiclePos.X - (textSize.x / 2), vehiclePos.Y}, IM_COL32(255, 255, 255, 255), s.c_str());
						}
					}
				}
			}
		}
	}
	g_LocalController = localController;
	g_LocalPlayer = localPlayer;
	{
		std::string s = "E [" + std::to_string((int)totalEnemies) + "] | [" + std::to_string((int)totalBots) + "] B";
		ImVec2 vStart = {glWidth / 2 - 80, 54};
		ImVec2 vEnd = {glWidth / 2 + 80, 104};
		ImVec2 rectCenter = ImVec2((vStart.x + vEnd.x) / 2.0f, (vStart.y + vEnd.y) / 2.0f);
		ImVec2 textSize = ImGui::CalcTextSize2(s.c_str(), 0, ((float)density / 16.5f));
		ImVec2 textPos = ImVec2(rectCenter.x - textSize.x / 2.0f, rectCenter.y - textSize.y / 2.0f);
		draw->AddRectFilled(vStart, vEnd, totalEnemies + totalBots > 0 ? IM_COL32(255, 165, 0, 120) : IM_COL32(0, 255, 0, 120), 5.0f);
		draw->AddRect(vStart, vEnd, totalEnemies + totalBots > 0 ? IM_COL32(255, 165, 0, 80) : IM_COL32(0, 255, 0, 80), 5.0f, 0, 2.5f);
		draw->AddText(NULL, ((float)density / 16.5f), textPos, IM_COL32(0, 0, 0, 255), s.c_str());
	}
}

std::string getClipboardText() {
    if (!g_App)
        return "";
    auto activity = g_App->activity;
    if (!activity)
        return "";
    auto vm = activity->vm;
    if (!vm)
        return "";
    auto object = activity->clazz;
    if (!object)
        return "";
    std::string result;
    JNIEnv * env;
    vm->AttachCurrentThread( & env, 0);
    {
        auto ContextClass = env->FindClass("android/content/Context");
        auto getSystemServiceMethod = env->GetMethodID(ContextClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
        auto str = env->NewStringUTF("clipboard");
        auto clipboardManager = env->CallObjectMethod(object, getSystemServiceMethod, str);
        env->DeleteLocalRef(str);
        auto ClipboardManagerClass = env->FindClass("android/content/ClipboardManager");
        auto getText = env->GetMethodID(ClipboardManagerClass, "getText", "()Ljava/lang/CharSequence;");
        auto CharSequenceClass = env->FindClass("java/lang/CharSequence");
        auto toStringMethod = env->GetMethodID(CharSequenceClass, "toString", "()Ljava/lang/String;");
        auto text = env->CallObjectMethod(clipboardManager, getText);
        if (text) {
            str = (jstring)env->CallObjectMethod(text, toStringMethod);
            result = env->GetStringUTFChars(str, 0);
            env->DeleteLocalRef(str);
            env->DeleteLocalRef(text);
        }
        env->DeleteLocalRef(CharSequenceClass);
        env->DeleteLocalRef(ClipboardManagerClass);
        env->DeleteLocalRef(clipboardManager);
        env->DeleteLocalRef(ContextClass);
    }
    vm->DetachCurrentThread();
    return result;
}
const char * GetAndroidID(JNIEnv * env, jobject context) {
    jclass contextClass = env->FindClass("android/content/Context");
    jmethodID getContentResolverMethod = env->GetMethodID(contextClass, "getContentResolver", "()Landroid/content/ContentResolver;");
    jclass settingSecureClass = env->FindClass("android/provider/Settings$Secure");
    jmethodID getStringMethod = env->GetStaticMethodID(settingSecureClass, "getString", "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;");
    auto obj = env->CallObjectMethod(context, getContentResolverMethod);
    auto str = (jstring)env->CallStaticObjectMethod(settingSecureClass, getStringMethod, obj, env->NewStringUTF("android_id"));
    return env->GetStringUTFChars(str, 0);
}
const char * GetDeviceModel(JNIEnv * env) {
    jclass buildClass = env->FindClass("android/os/Build");
    jfieldID modelId = env->GetStaticFieldID(buildClass, "MODEL", "Ljava/lang/String;");
    auto str = (jstring)env->GetStaticObjectField(buildClass, modelId);
    return env->GetStringUTFChars(str, 0);
}
const char * GetDeviceBrand(JNIEnv * env) {
    jclass buildClass = env->FindClass("android/os/Build");
    jfieldID modelId = env->GetStaticFieldID(buildClass, "BRAND", "Ljava/lang/String;");
    auto str = (jstring)env->GetStaticObjectField(buildClass, modelId);
    return env->GetStringUTFChars(str, 0);
}
const char * GetDeviceUniqueIdentifier(JNIEnv * env, const char * uuid) {
    jclass uuidClass = env->FindClass("java/util/UUID");
    auto len = strlen(uuid);
    jbyteArray myJByteArray = env->NewByteArray(len);
    env->SetByteArrayRegion(myJByteArray, 0, len, (jbyte *)uuid);
    jmethodID nameUUIDFromBytesMethod = env->GetStaticMethodID(uuidClass, "nameUUIDFromBytes", "([B)Ljava/util/UUID;");
    jmethodID toStringMethod = env->GetMethodID(uuidClass, "toString", "()Ljava/lang/String;");
    auto obj = env->CallStaticObjectMethod(uuidClass, nameUUIDFromBytesMethod, myJByteArray);
    auto str = (jstring)env->CallObjectMethod(obj, toStringMethod);
    return env->GetStringUTFChars(str, 0);
}
struct MemoryStruct {
    char * memory;
    size_t size;
};
static size_t WriteMemoryCallback(void * contents, size_t size, size_t nmemb, void * userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct * mem = (struct MemoryStruct *)userp;
    mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        return 0;
    }
    memcpy( & (mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

std::string Login(const char * user_key) {
    if (!g_App)
        return OBFUSCATE("Internal Error");
    auto activity = g_App->activity;
    if (!activity)
        return OBFUSCATE("Internal Error");
    auto vm = activity->vm;
    if (!vm)
        return OBFUSCATE("Internal Error");
    auto object = activity->clazz;
    if (!object)
        return OBFUSCATE("Internal Error");
	if (Tools::IsApkInstalled(OBFUSCATE("com.guoshi.httpcanary")) || Tools::IsApkInstalled(OBFUSCATE("com.guoshi.httpcanary.premium")) || Tools::IsApkInstalled(OBFUSCATE("com.sniffer"))) {
		return OBFUSCATE("Uninstall Http Canary & Open Again");
	} else if (Tools::IsVpnActive()) {
		return OBFUSCATE("Turn Off VPN & Open Again");
	} else {
		JNIEnv * env;
		vm->AttachCurrentThread( & env, 0);
		std::string hwid = user_key;
		hwid += GetAndroidID(env, object);
		hwid += GetDeviceModel(env);
		hwid += GetDeviceBrand(env);
		std::string UUID = GetDeviceUniqueIdentifier(env, hwid.c_str());
		std::string errMsg;
		vm->DetachCurrentThread();
		struct MemoryStruct chunk { };
		chunk.memory = (char *)malloc(1);
		chunk.size = 0;
		CURL * curl;
		CURLcode res;
		curl = curl_easy_init();
		if (curl) {
			curl_easy_setopt(curl, CURLOPT_URL, /*https://api.a-modz.in/am/public/connect*/ StrEnc("l7;YC.QmupFe*pt_,FP+e_K.0*?5!E2S9- <:E~", "\x04\x43\x4F\x29\x30\x14\x7E\x42\x14\x00\x2F\x4B\x4B\x5D\x19\x30\x48\x3C\x7E\x42\x0B\x70\x2A\x43\x1F\x5A\x4A\x57\x4D\x2C\x51\x7C\x5A\x42\x4E\x52\x5F\x26\x0A", 39).c_str());
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, /*https*/ StrEnc("!mLBO", "\x49\x19\x38\x32\x3C", 5).c_str());
			struct curl_slist * headers = NULL;
			headers = curl_slist_append(headers, /*Content-Type: application/x-www-form-urlencoded*/ StrEnc("@;Ls\\(KP4Qrop`b#d3094/r1cf<c<=H)AiiBG6i|Ta66s2[", "\x03\x54\x22\x07\x39\x46\x3F\x7D\x60\x28\x02\x0A\x4A\x40\x03\x53\x14\x5F\x59\x5A\x55\x5B\x1B\x5E\x0D\x49\x44\x4E\x4B\x4A\x3F\x04\x27\x06\x1B\x2F\x6A\x43\x1B\x10\x31\x0F\x55\x59\x17\x57\x3F", 47).c_str());
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			char data[4096];
			//sprintf(data, /*game=PUBG&user_key=%s&serial=%s*/ StrEnc("qu2yXK,YkJyGD@ut0.u~Nb'5(:.:chK", "\x16\x14\x5F\x1C\x65\x1B\x79\x1B\x2C\x6C\x0C\x34\x21\x32\x2A\x1F\x55\x57\x48\x5B\x3D\x44\x54\x50\x5A\x53\x4F\x56\x5E\x4D\x38", 31).c_str(), user_key, UUID.c_str());
			//sprintf(data, /*game=BETALITE&user_key=%s&serial=%s*/ StrEnc("QIx\\( $.CH0t\\fZ:!*gCtjSXADYoG>(]V>S", "\x36\x28\x15\x39\x15\x62\x61\x7A\x02\x04\x79\x20\x19\x40\x2F\x49\x44\x58\x38\x28\x11\x13\x6E\x7D\x32\x62\x2A\x0A\x35\x57\x49\x31\x6B\x1B\x20", 35).c_str(), user_key, UUID.c_str());
			sprintf(data, /*game=PUBGLITE&user_key=%s&serial=%s*/ StrEnc("do(?Q)im>{{k%_K%eKd\\/#I)u-[nh'9$|uE", "\x03\x0E\x45\x5A\x6C\x79\x3C\x2F\x79\x37\x32\x3F\x60\x79\x3E\x56\x00\x39\x3B\x37\x4A\x5A\x74\x0C\x06\x0B\x28\x0B\x1A\x4E\x58\x48\x41\x50\x36", 35).c_str(), user_key, UUID.c_str());
			//sprintf(data, /*game=PUBGEXT&user_key=%s&serial=%s*/ StrEnc("o+!+CO{6$U^xstJ89_?ay|lE5!l%@fS]$(", "\x08\x4A\x4C\x4E\x7E\x1F\x2E\x74\x63\x10\x06\x2C\x55\x01\x39\x5D\x4B\x00\x54\x04\x00\x41\x49\x36\x13\x52\x09\x57\x29\x07\x3F\x60\x01\x5B", 34).c_str(), user_key, UUID.c_str());
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) & chunk);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
			res = curl_easy_perform(curl);
			if (res == CURLE_OK) {
				try {
					json result = json::parse(chunk.memory);
					if (result[/*status*/ StrEnc("(>_LBm", "\x5B\x4A\x3E\x38\x37\x1E", 6).c_str()] == true) {
						std::string token = result[/*data*/ StrEnc("fAVA", "\x02\x20\x22\x20", 4).c_str()][/*token*/ StrEnc("{>3Lr", "\x0F\x51\x58\x29\x1C", 5).c_str()].get<std::string>();
						time_t rng = result[/*data*/ StrEnc("fAVA", "\x02\x20\x22\x20", 4).c_str()][/*rng*/ StrEnc("+n,", "\x59\x00\x4B", 3).c_str()].get<time_t>();
						if (rng + 30 > time(0)) {
							//std::string auth = /*PUBG*/ StrEnc("Q*) ", "\x01\x7F\x6B\x67", 4).c_str();
							//std::string auth = /*BETALITE*/ StrEnc("#)S`Cb=U", "\x61\x6C\x07\x21\x0F\x2B\x69\x10", 8).c_str();
							std::string auth = /*PUBGLITE*/ StrEnc("z9@.;<s/", "\x2A\x6C\x02\x69\x77\x75\x27\x6A", 8).c_str();
							//std::string auth = /*PUBGEXT*/ StrEnc("d6pj[%o", "\x34\x63\x32\x2D\x1E\x7D\x3B", 7).c_str();
							auth += "-";
							auth += user_key;
							auth += "-";
							auth += UUID;
							auth += "-";
							auth += /*KjBSjwu737d6sxjiHAg674b98s376*/ StrEnc("AtZ.2*R&.,&J|~WL\\g4_s.t,Ig?-\\", "\x0A\x1E\x18\x7D\x58\x5D\x27\x11\x1D\x1B\x42\x7C\x0F\x06\x3D\x25\x14\x26\x53\x69\x44\x1A\x16\x15\x71\x14\x0C\x1A\x6A", 29).c_str();
							std::string outputAuth = Tools::CalcMD5(auth);
							g_Token = token;
							g_Auth = outputAuth;
							bValid = g_Token == g_Auth;
						}
					} else {
						errMsg = result[/*reason*/ StrEnc("LW(3(c", "\x3E\x32\x49\x40\x47\x0D", 6).c_str()].get<std::string>();
					}
				} catch (json::exception & e) {
					errMsg = "{";
					errMsg += e.what();
					errMsg += "}\n{";
					errMsg += chunk.memory;
					errMsg += "}";
				}
			} else {
				errMsg = curl_easy_strerror(res);
			}
		}
		curl_easy_cleanup(curl);
		vm->DetachCurrentThread();
		return bValid ? StrEnc("8q", "\x77\x3A", 2).c_str() : errMsg;
	}
	return OBFUSCATE("Internal Error");
}

void * run_thread(void * arguments) {
	switch ((int)arguments) {
	case 1:
		if (Config.Mem.iBypass) {
		//	g_LocalPlayer->GameSession->MaxSplitscreensPerConnection = 0;
			MemoryPatch::createWithHex("libgcloud.so", 0x3B4A8, "1E FF 2F E1").Modify();
			MemoryPatch::createWithHex("libgcloud.so", 0x3B4B0, "1E FF 2F E1").Modify();
			MemoryPatch::createWithHex("libgcloud.so", 0x62FFC, "1E FF 2F E1").Modify();
			Config.PlayerESP.Line = true;
			Config.PlayerESP.Box = true;
			Config.PlayerESP.Health = true;
			//copyLibrary();
			Config.PlayerESP.Name = true;
			Config.PlayerESP.Distance = true;
			Config.PlayerESP.TeamID = true;
		} else {
			MemoryPatch::createWithHex("libgcloud.so", 0x3B4A8, "03 C6 8F E2").Modify();
			MemoryPatch::createWithHex("libgcloud.so", 0x3B4B0, "1C F6 BC E5").Modify();
			MemoryPatch::createWithHex("libgcloud.so", 0x62FFC, "89 EA 04 04").Modify();
			Config.PlayerESP.Line = false;
			Config.PlayerESP.Box = false;
			Config.PlayerESP.Health = false;
			Config.PlayerESP.Name = false;
			Config.PlayerESP.Distance = false;
			Config.PlayerESP.TeamID = false;
			Config.Mem.gvPeekDown = false;
			Config.Mem.gvPeekUp = false;
		}
		break;
	case 2:
		if (Config.Mem.flashv1) {
			MemoryPatch::createWithHex("libUE4.so", 0x2C3BA14, "61 D9 08 41 2D 43 1C EB").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2704F44, "9A 99 99 3E 71 0A 90 ED").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2D107D0, "E2 7F 89 12 11 0A 01 EE").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2E8CED4, "00 00 B4 41 16 EF C3 3E").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2C3BA14, "61 D9 08 41").Modify();//10.9
			MemoryPatch::createWithHex("libUE4.so", 0xCBCB00, "00 00 50 E3").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2704F44, "9A 99 99 3E").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2710C8C, "00 00 00 00").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2C3B974, "00 00 00 00").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2C3B98C, "00 00 00 00").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2C3BA18, "00 00 00 00").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2D107D0, "00 00 00 00").Modify();
		} else {
			MemoryPatch::createWithHex("libUE4.so", 0x2C3BA14, "80 84 2E 41 2D 43 1C EB").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2704F44, "BD 37 86 35 71 0A 90 ED").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2D107D0, "A5 0A 9F ED 10 0A 01 EE").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2E8CED4, "65 CD CD 41").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2C3BA14, "80 84 2E 41").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0xCBCB00, "01 00 50 E3").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2704F44, "5A B0 BB A4").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2710C8C, "48 0A B0 9E").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2C3B974, "59 00 00 0A").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2C3B98C, "53 00 00 1A").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2C3BA18, "2D 43 1C EB").Modify();
			MemoryPatch::createWithHex("libUE4.so", 0x2D107D0, "A5 0A 9F ED").Modify();
		}
		break;
	case 3:
		if (Config.Mem.flashv2) {
			Patches.oldflash1.Modify();
			Patches.oldflash2.Modify();
			Patches.oldflash3.Modify();
			Patches.oldflash4.Modify();
			Patches.oldflash5.Modify();
			Patches.oldflash6.Modify();
			Patches.oldflash7.Modify();
			Patches.oldflash8.Modify();
			Patches.oldflash9.Modify();
		} else {
			Patches.oldflash1.Restore();
			Patches.oldflash2.Restore();
			Patches.oldflash3.Restore();
			Patches.oldflash4.Restore();
			Patches.oldflash5.Restore();
			Patches.oldflash6.Restore();
			Patches.oldflash7.Restore();
			Patches.oldflash8.Restore();
			Patches.oldflash9.Restore();
		}
		break;
	case 4:
		if (Config.Mem.fixstuck) {
			g_LocalPlayer->StandHalfHeight = 77.0f;
			g_LocalPlayer->CrouchHalfHeight = -63.0f;
		} else {
			g_LocalPlayer->StandHalfHeight = 88.0f;
			g_LocalPlayer->CrouchHalfHeight = 60.0f;
		}
		break;
	case 5:
		if (Config.Mem.magic) {
		    SetSearchRange(Range::A_ANONMYOUS);
			MemorySearch("9.20161819458", Type::MEM_FLOAT);
			MemoryOffset("25.0", 28, Type::MEM_FLOAT);
			MemoryOffset("30.5", 32, Type::MEM_FLOAT);
			MemoryWrite("250", 28, Type::MEM_FLOAT);
			MemoryWrite("255.5", 32, Type::MEM_FLOAT);
			ClearResults();
			/*MemorySearch("69.5", Type::MEM_FLOAT);
            MemoryOffset("33.0", -4, Type::MEM_FLOAT);
            MemoryOffset("35.0", -8, Type::MEM_FLOAT);
            MemoryWrite("89.5", 0, Type::MEM_FLOAT);
			ClearResults();*/
		}
		break;
	case 6:
		if (Config.Mem.long_hand) {
			SetSearchRange(Range::A_ANONMYOUS);
			MemorySearch("0.0060901641845703125", Type::MEM_FLOAT);
			MemoryOffset("2.2840499877929688E-4", -4, Type::MEM_FLOAT);
			MemoryOffset("23.52225685119629", -8, Type::MEM_FLOAT);
			MemoryWrite("200", -4, Type::MEM_FLOAT);
			MemoryWrite("100", -8, Type::MEM_FLOAT);
			ClearResults();
		} else {
		    SetSearchRange(Range::A_ANONMYOUS);
			MemorySearch("0.0060901641845703125", Type::MEM_FLOAT);
			MemoryOffset("200", -4, Type::MEM_FLOAT);
			MemoryOffset("100", -8, Type::MEM_FLOAT);
			MemoryWrite("2.2840499877929688E-4", -4, Type::MEM_FLOAT);
			MemoryWrite("23.52225685119629", -8, Type::MEM_FLOAT);
			ClearResults();
		}
		break;
		
	case 7:
		if (Config.Mem.skyfly) {
	g_LocalPlayer->CharacterMovement->GravityScale = 0;
		g_LocalPlayer->CharacterMovement->MaxFlySpeed=99;
			g_LocalPlayer->CharacterMovement->JumpZVelocity = 600;
		g_LocalPlayer->CharacterMovement->JumpOffJumpZFactor = 600;
	/*	g_LocalPlayer->CharacterMovement->MaxWalkSpeed = 999;
			g_LocalPlayer->CharacterMovement->MaxWalkSpeedCrouched = 999;
			g_LocalPlayer->CharacterMovement->MaxSwimSpeed = 999;*/
		g_LocalPlayer->STCharacterMovement->AirControl =10000;
        g_LocalPlayer->STCharacterMovement->AirControlBoostMultiplier = 100;
        g_LocalPlayer->STCharacterMovement->AirControlBoostVelocityThreshold = 100;
		
		
		//SetJumpZVelocityFromFile();
			* (bool *)((uintptr_t)g_LocalPlayer + 0x364) = true;
		} else {
			g_LocalPlayer->CharacterMovement->GravityScale = 0;
		g_LocalPlayer->CharacterMovement->MaxFlySpeed=99;
			g_LocalPlayer->CharacterMovement->JumpZVelocity = 0;
			* (bool *)((uintptr_t)g_LocalPlayer + 0x364) = false;
		}
		break;
	case 8:
		if (Config.Mem.fastland) {
			
			SetSearchRange(Range::A_ANONMYOUS);
   MemorySearch("-1233361362001362892", Type::MEM_QWORD);
   MemoryOffset("4770437906357551104", 0x1AC, Type::MEM_QWORD);
   MemoryWrite("5006313939071729664", 0x1AC, Type::MEM_QWORD);
   ClearResults();
  //  g_LocalPlayer->ParachuteComponent->MaxZAllowDistance = 40000;
		    SetSearchRange(Range::A_ANONMYOUS);
			MemorySearch("3000.0", Type::MEM_FLOAT);
			MemoryOffset("5000.0", 4, Type::MEM_FLOAT);
			MemoryOffset("1024.0", 8, Type::MEM_FLOAT);
			MemoryOffset("89.0", 36, Type::MEM_FLOAT);
			MemoryWrite("200000", 0, Type::MEM_FLOAT);
			MemoryWrite("200000", 4, Type::MEM_FLOAT);
			MemoryWrite("200000", 8, Type::MEM_FLOAT);
			ClearResults();
		}
		break;
	case 9:
		if (Config.Mem.slowMo) {
			g_LocalPlayer->STPlayerController->AcknowledgedPawn->CustomTimeDilation = 0.25f;
		} else {
			g_LocalPlayer->STPlayerController->AcknowledgedPawn->CustomTimeDilation = 1.0f;
		}
		break;
	case 10:
		if (Config.Mem.headShoot) {
			SetSearchRange(Range::A_ANONMYOUS);
			MemorySearch("-92.2311706543", Type::MEM_FLOAT);
			MemoryOffset("16.0", 0x4, Type::MEM_FLOAT);
			MemoryOffset("26.0", 0x8, Type::MEM_FLOAT);
			MemoryWrite("-1339.0", 0x8, Type::MEM_FLOAT);
			ClearResults();
			SetSearchRange(Range::A_ANONMYOUS);
			MemorySearch("-90.74298858643", Type::MEM_FLOAT);
			MemoryOffset("18.0", 0x4, Type::MEM_FLOAT);
			MemoryOffset("27.25", 0x8, Type::MEM_FLOAT);
			MemoryWrite("-1339.0", 0x8, Type::MEM_FLOAT);	// 26
			ClearResults();
		}
		break;

    //switch (case11) {
        case 11:
		  if (Config.Mem.baba) {
       g_LocalPlayer->UPhysicsSettings->DefaultGravityZ = 0; //Gravilty
		g_LocalPlayer->UPhysicsSettings->DefaultTerminalVelocity = 0;
		g_LocalPlayer->UPhysicsSettings->DefaultFluidFriction = 0;
		//if (Config.HighRisk.Unlock) {
/*if (Object->IsA(USTExtraGameInstance::StaticClass())) {
auto g_LocalPlayer = (USTExtraGameInstance *) Object;      
g_LocalPlayer->USTExtraGameInstance->UserDetailSetting.PUBGDeviceFPSDef = 90.0f;   
g_LocalPlayer->USTExtraGameInstance->UserDetailSetting.PUBGDeviceFPSLow = 90.0f;
g_LocalPlayer->USTExtraGameInstance->UserDetailSetting.PUBGDeviceFPSMid = 90.0f;
g_LocalPlayer->USTExtraGameInstance->UserDetailSetting.PUBGDeviceFPSHigh = 90.0f;
g_LocalPlayer->USTExtraGameInstance->UserDetailSetting.PUBGDeviceFPSHDR = 90.0f;
g_LocalPlayer->USTExtraGameInstance->UserDetailSetting.PUBGDeviceFPSUltralHigh = 90.0f;/*

          /*  std::vector<std::string> libs = {"libUE4.so:bss", "Cb"}; 
            
            std::vector<uintptr_t> offsets1 = {0x62EB8, 0xA8, 0x24C, 0x24};
            uintptr_t address1 = calculateAddress(libs[0], libs[1], offsets1);
            setValue(address1, 1000000); // 开枪防拉

            std::vector<uintptr_t> offsets2 = {0x1EADC8, 0x20, 0x2CC, 0x1558};
            uintptr_t address2 = calculateAddress(libs[0], libs[1], offsets2);
            setValue(address2, 75); // 浮

            std::vector<uintptr_t> offsets3 = {0x1EADC8, 0x20, 0x18, 0x1C8, 0x3C8};
            uintptr_t address3 = calculateAddress(libs[0], libs[1], offsets3);
            setValue(address3, 1.2f); // 加速--射速

            std::vector<uintptr_t> offsets4 = {0x1EADC8, 0x20, 0x2CC, 0x310, 0x138};
            uintptr_t address4 = calculateAddress(libs[0], libs[1], offsets4);
            setValue(address4, 60); // 35

            std::vector<uintptr_t> offsets5 = {0x1EADC8, 0x20, 0x2CC, 0x310, 0x1AC};
            uintptr_t address5 = calculateAddress(libs[0], libs[1], offsets5);
            setValue(address5, 99999); // 蛇皮跳跃

            std::vector<uintptr_t> offsets6 = {0x1EADC8, 0x20, 0x2CC, 0x60};
            uintptr_t address6 = calculateAddress(libs[0], libs[1], offsets6);
            setValue(address6, 1.36f); // 减速--空中关键速度*/
        }
        break;
    default:
        break;
    }
    return nullptr;
}


void startThread(int argument) {
    std::thread thread(run_thread, (void*)argument);
    thread.detach();
}

EGLBoolean( * orig_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
EGLBoolean _eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
	eglQuerySurface(dpy, surface, EGL_WIDTH, & glWidth);
	eglQuerySurface(dpy, surface, EGL_HEIGHT, & glHeight);
	if (glWidth <= 0 || glHeight <= 0)
		return orig_eglSwapBuffers(dpy, surface);
	if (!g_App)
		return orig_eglSwapBuffers(dpy, surface);
	screenWidth = ANativeWindow_getWidth(g_App->window);
	screenHeight = ANativeWindow_getHeight(g_App->window);
	density = AConfiguration_getDensity(g_App->config);
	if (!initImGui) {
		ImGui::CreateContext();
		ImGuiStyle * style = & ImGui::GetStyle();
		ImGui::StyleColorsClassic(style);
		style->WindowRounding = 4.0f;
		style->FramePadding = ImVec2(4, 5);
		style->FrameRounding = 5.0f;
		style->ScaleAllSizes(std::max(2.0f, density / 400.0f));
		ImGui_ImplAndroid_Init();
		ImGui_ImplOpenGL3_Init("#version 300 es");
		ImGuiIO & io = ImGui::GetIO();
		io.ConfigWindowsMoveFromTitleBarOnly = true;
		io.IniFilename = NULL;
		ImFontConfig cfg;
		cfg.SizePixels = std::max(21.33f, ((float)density / 22.5f));
		io.Fonts->AddFontDefault( & cfg);
		memset( & Config, 0, sizeof(sConfig));
		initImGui = true;
	}
	ImGuiIO & io = ImGui::GetIO();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplAndroid_NewFrame(glWidth, glHeight);
	ImGui::NewFrame();
	//if (bValid)
		DrawESPVIP(ImGui::GetBackgroundDrawList());
	ImGui::SetNextWindowSize(ImVec2((float)glWidth * 0.35f, (float)glHeight * 0.62f), ImGuiCond_Once);
	if (ImGui::Begin(OBFUSCATE("AkshatModz Module (x32) - v14.0 ::  @AkshatModz_Org"), 0, ImGuiWindowFlags_NoSavedSettings)) {
		static bool isLogin = true;
		if (!isLogin) {
			static std::string err;
			ImGui::Text(OBFUSCATE("Please Login! (Copy Key to Clipboard)"));
			ImGui::PushItemWidth(-1);
			static char s[64];
			ImGui::InputText(OBFUSCATE("##key"), s, sizeof s);
			// auto paste
			auto key = getClipboardText();
			strncpy(s, key.c_str(), sizeof s);
			// auto login
			err = Login(s);
			if (err == "OK") {
				isLogin = bValid && g_Auth == g_Token;
			}
			ImGui::PopItemWidth();
			ImGui::PushItemWidth(-1);
			if (ImGui::Button(OBFUSCATE(" Paste Key  "), ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
				auto key = getClipboardText();
				strncpy(s, key.c_str(), sizeof s);
			}
			ImGui::PopItemWidth();
			ImGui::PushItemWidth(-1);
			if (ImGui::Button(OBFUSCATE("Login"), ImVec2(ImGui::GetContentRegionAvailWidth(), 0))) {
				err = Login(s);
				if (err == "OK") {
					isLogin = bValid && g_Auth == g_Token;
				}
			}
			ImGui::PopItemWidth();
			if (!err.empty() && err != "OK") {
				ImGui::Text(OBFUSCATE("Error: %s"), err.c_str());
			}
			ImGui::PopItemWidth();
		} else {
			if (ImGui::BeginTabBar("Tab", ImGuiTabBarFlags_FittingPolicyScroll)) {
				if (ImGui::BeginTabItem("Visuals")) {
					if (ImGui::Checkbox("One Click Active + Island Bypass", & Config.Mem.iBypass)) {
						startThread(1);
					}
					DrawTextCentered("ESP Player");
					if (ImGui::BeginTable("split", 3)) {
						ImGui::TableNextColumn();
						ImGui::Checkbox("Line", & Config.PlayerESP.Line);
						ImGui::TableNextColumn();
						ImGui::Checkbox("Box", & Config.PlayerESP.Box);
						ImGui::TableNextColumn();
						ImGui::Checkbox("Skeleton", & Config.PlayerESP.Skeleton);
						ImGui::TableNextColumn();
						ImGui::Checkbox("Health", & Config.PlayerESP.Health);
						ImGui::TableNextColumn();
						ImGui::Checkbox("Name", & Config.PlayerESP.Name);
						ImGui::TableNextColumn();
						ImGui::Checkbox("Distance", & Config.PlayerESP.Distance);
						ImGui::TableNextColumn();
						ImGui::Checkbox("Team ID", & Config.PlayerESP.TeamID);
						ImGui::TableNextColumn();
						ImGui::Checkbox("360 Alert", & Config.PlayerESP.Alert360);
						ImGui::TableNextColumn();
						ImGui::Checkbox("Ignore Bot", & Config.PlayerESP.NoBot);
						ImGui::TableNextColumn();
						ImGui::Checkbox("Show Vehicle", & Config.PlayerESP.Vehicle);
						ImGui::EndTable();
					}
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Weapons")) {
					ImGui::Spacing();
					for (auto & i : items_data) {
						if (ImGui::CollapsingHeader(i["Category"].get<std::string>().c_str())) {
							if (ImGui::BeginTable("split", 2)) {
								for (auto & item : i["Items"]) {
									ImGui::TableNextColumn();
									ImGui::Checkbox(item["itemName"].get<std::string>().c_str(), (bool *) & Items[item["itemId"].get<int>()]);
								}
								ImGui::EndTable();
							}
						}
					}
					ImGui::EndTabItem();
				}
                if (ImGui::BeginTabItem("Bullet Track")) {
                    static const char * ranges[] = {"180°", "360°"};
                    static const char * targets[] = {"Head", "Chest"};
                    static const char *triggers[] = {"None", "Shooting", "Scoping", "Both (Shooting & Scoping)", "Any (Shooting / Scoping)"};
                    DrawTextCentered("Bullet Tracking");
                    if (ImGui::BeginTable("split", 2)) {
                        ImGui::TableNextColumn();
                        ImGui::Checkbox("Enable BT", & Config.SilentAim.Enable);
                        ImGui::TableNextColumn();
                        ImGui::Checkbox("Auto Fire", & Config.SilentAim.AutoFire);
						ImGui::EndTable();
                    }
                    ImGui::Text("Range: ");
                    ImGui::SameLine();
                    ImGui::Combo("##Range", (int *) & Config.SilentAim.Range, ranges, 2, -1);
                    ImGui::Text("Target:");
                    ImGui::SameLine();
                    ImGui::Combo("##Target", (int *) & Config.SilentAim.Target, targets, 2, -1);
                    ImGui::Text("Trigger: ");
					ImGui::SameLine();
					ImGui::Combo("##Trigger", (int *) & Config.SilentAim.Trigger, triggers, 5, -1);
					if (ImGui::BeginTable("split", 2)) {
						ImGui::TableNextColumn();
                        ImGui::Checkbox("Visibility Check", & Config.SilentAim.VisCheck);
						ImGui::TableNextColumn();
                        ImGui::Checkbox("Target Line", & Config.SilentAim.TargetLine);
						ImGui::TableNextColumn();
                        ImGui::Checkbox("Ignore Knocked", & Config.SilentAim.IgnoreKnocked);
                        ImGui::TableNextColumn();
                        ImGui::Checkbox("Ignore Bot", & Config.SilentAim.IgnoreBot);
                        ImGui::EndTable();
                    }
                    ImGui::EndTabItem();
                }
				if (ImGui::BeginTabItem("Memory")) {
					DrawTextCentered("Memory");
                    if (ImGui::BeginTable("split", 2)) {
                        ImGui::TableNextColumn();
                        if (ImGui::Checkbox("Flash V1", & Config.Mem.flashv1)) {
							startThread(2);
						}
						ImGui::TableNextColumn();
                        if (ImGui::Checkbox("Flash V2", & Config.Mem.flashv2)) {
							startThread(3);
						}
						ImGui::TableNextColumn();
                        if (ImGui::Checkbox("Sit Fly", & Config.Mem.fixstuck)) {
							startThread(4);
						}
						ImGui::TableNextColumn();
                        if (ImGui::Checkbox("Slow Motion", & Config.Mem.slowMo)) {
							startThread(9);
						}
						ImGui::TableNextColumn();
                        if (ImGui::Checkbox("Magic Bullet", & Config.Mem.magic)) {
							startThread(5);
						}
						ImGui::TableNextColumn();
                        if (ImGui::Checkbox("HeadShoot", & Config.Mem.headShoot)) {
							startThread(10);
						}
						ImGui::TableNextColumn();
                        ImGui::Checkbox("God View Up", & Config.Mem.gvPeekUp);
                        ImGui::TableNextColumn();
                        ImGui::Checkbox("God View Down", & Config.Mem.gvPeekDown);
						ImGui::TableNextColumn();
                        if (ImGui::Checkbox("Long Hand", & Config.Mem.long_hand)) {
							startThread(6);
						}
						ImGui::TableNextColumn();
                        if (ImGui::Checkbox("Sky Fly", & Config.Mem.skyfly)) {
							startThread(7);
						}
						ImGui::TableNextColumn();
                        if (ImGui::Checkbox("Fast Land", & Config.Mem.fastland)) {
							startThread(8);
						}
						ImGui::TableNextColumn();
                        ImGui::Checkbox("No Recoil", & Config.Mem.Recoil);
                        ImGui::TableNextColumn();
                        ImGui::Checkbox("No Shake", & Config.Mem.Shake);
                        ImGui::TableNextColumn();
                        ImGui::Checkbox("Small Crosshair", & Config.Mem.sCross);
						     ImGui::TableNextColumn();
                        ImGui::Checkbox("Testing", & Config.Mem.baba);
						ImGui::EndTable();
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
    }
    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return orig_eglSwapBuffers(dpy, surface);
}

int32_t ( * orig_onInputEvent)(struct android_app * app, AInputEvent * inputEvent);
int32_t onInputEvent(struct android_app * app, AInputEvent * inputEvent) {
    if (initImGui) {
        ImGui_ImplAndroid_HandleInputEvent(inputEvent, {(float)screenWidth / (float)glWidth, (float)screenHeight / (float)glHeight});
    }
    return orig_onInputEvent(app, inputEvent);
}

#define SLEEP_TIME (1000LL / 60LL)
[[noreturn]] void * maps_thread(void *) {
	while (true) {
		auto t1 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		auto localPlayer = g_LocalPlayer;
		if (localPlayer) {
			if (Config.Mem.gvPeekUp) {
				if (localPlayer->IsPeekLeft)  {
					auto root = * (int *)((uintptr_t)localPlayer + 0x140) + 0x158;
					float ground;
					Tools::PVM_ReadAddr((void *)(root), & ground, sizeof(ground));
					while (true) {
						Write<float>(root, ground + 300.0f);
						if (!localPlayer->IsPeekLeft || localPlayer->Health == 0.0f)
							break;
					}
				}
			}
			if (Config.Mem.gvPeekDown) {
				if (localPlayer->IsPeekLeft)  {
					auto root = * (int *)((uintptr_t)localPlayer + 0x140) + 0x158;
					float ground;
					Tools::PVM_ReadAddr((void *)(root), & ground, sizeof(ground));
					while (true) {
						Write<float>(root, ground - 250.0f);
						if (!localPlayer->IsPeekLeft || localPlayer->Health == 0.0f)
							break;
					}
				}
			}
		}
		auto td = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - t1;
		std::this_thread::sleep_for(std::chrono::milliseconds(std::max(std::min(0LL, SLEEP_TIME - td), SLEEP_TIME)));
	}
}




void * main_thread(void *) {
    UE4 = Tools::GetBaseAddress("libUE4.so");
    while (!UE4) {
        UE4 = Tools::GetBaseAddress("libUE4.so");
		copyLibrary();
        sleep(1);
    }
    while (!g_App) {
        g_App = * (android_app * *)(UE4 + GNativeAndroidApp_Offset);
        sleep(1);
    }
    while (!g_App->onInputEvent)
        sleep(1);
    orig_onInputEvent = decltype(orig_onInputEvent)(g_App->onInputEvent);
    g_App->onInputEvent = onInputEvent;
    FName::GNames = GetGNames();
    while (!FName::GNames) {
        FName::GNames = GetGNames();
        sleep(1);
    }
    UObject::GUObjectArray = (FUObjectArray *)(UE4 + GUObject_Offset);
    plthook_t * plthook;
    if (plthook_open( & plthook, "libUE4.so") == 0) {
        plthook_replace(plthook, "eglSwapBuffers", (void *)_eglSwapBuffers, (void * *) & orig_eglSwapBuffers);
        plthook_close(plthook);
    }
	Tools::Hook((void *)(UE4 + 0x12c45b4), (void *) hsub_CalcShootRot, (void **) &osub_CalcShootRot);
	//Crash Fix
	MemoryPatch::createWithHex("libanogs.so", 0xACAD2, "00 20 70 47").Modify();
	MemoryPatch::createWithHex("libanogs.so", 0x27B3A6, "00 20 70 47").Modify();
	//Logo
	MemoryPatch::createWithHex("libanogs.so", 0x260D00, "00 20 70 47").Modify();//--10 years 
	MemoryPatch::createWithHex("libanogs.so", 0x73210, "00 20 70 47").Modify();//---3rd party
	MemoryPatch::createWithHex("libanogs.so", 0x8E6E4, "00 20 70 47").Modify();//---case 35
	MemoryPatch::createWithHex("libanogs.so", 0x7AF2C, "00 20 70 47").Modify();//---memory class
	MemoryPatch::createWithHex("libanogs.so", 0x4BAF8, "00 20 70 47").Modify();
	MemoryPatch::createWithHex("libanogs.so", 0x4B320, "00 20 70 47").Modify();
	MemoryPatch::createWithHex("libanogs.so", 0x898DC, "00 20 70 47").Modify();
	//Memory
	Patches.slowMo = MemoryPatch::createWithHex("libUE4.so", 0x26EA5D4, "00 00 00 00");
	Patches.oldflash1 = MemoryPatch::createWithHex("libUE4.so", 0x2C3BA14, "61 D9 08 41");
	Patches.oldflash2 = MemoryPatch::createWithHex("libUE4.so", 0xCBCB00, "00 00 50 E3");
	Patches.oldflash3 = MemoryPatch::createWithHex("libUE4.so", 0x2E8CED4, "00 00 20 40"); 
	Patches.oldflash4 = MemoryPatch::createWithHex("libUE4.so", 0x2704F44, "9A 99 99 3E");
	Patches.oldflash5 = MemoryPatch::createWithHex("libUE4.so", 0x2710C8C, "00 00 00 00");
	Patches.oldflash6 = MemoryPatch::createWithHex("libUE4.so", 0x2C3B974, "00 00 00 00");
	Patches.oldflash8 = MemoryPatch::createWithHex("libUE4.so", 0x2C3BA18, "00 00 00 00");
	Patches.oldflash9 = MemoryPatch::createWithHex("libUE4.so", 0x2D107D0, "00 00 00 00");
	items_data = json::parse(JSON_ITEMS);
	initMemTool();
	std::thread mapsThread(maps_thread, nullptr);
    mapsThread.detach();
	return 0;
}
/*#include "zygisk.hpp"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

void hack();

class MyModule : public zygisk::ModuleBase {
  public:
    void onLoad(Api *api, JNIEnv *env) override {
        this->api_ = api;
        this->env_ = env;
    }
    void preAppSpecialize(AppSpecializeArgs *args) override {
        static constexpr const char *packages[] = {"com.tencent.iglite"};
        const char *process = env_->GetStringUTFChars(args->nice_name, nullptr);
        for (const auto *package: packages) {
            is_game_ = (strcmp(process, package) == 0);
            if (is_game_) {
                break;
            }
        }
        env_->ReleaseStringUTFChars(args->nice_name, process);
    }
    void postAppSpecialize(const AppSpecializeArgs *args) override {
        if (is_game_) {
            std::thread{hack}.detach();
        }
    }
  private:
    Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
    bool is_game_ = false;
};
void hack() {
    pthread_t t;
    pthread_create(&t, 0, main_thread, 0);
}
REGISTER_ZYGISK_MODULE(MyModule)*/
__attribute__((constructor)) void _init() {
    std::thread mainThread(main_thread, nullptr);
    mainThread.detach();
}

