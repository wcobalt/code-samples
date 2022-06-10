// Copyright Flying Wild Hog. All Rights Reserved.

#include "GamepadDetector.h"
#include "HAL/Platform.h"
#include <regex>
#include "GenericPlatform/GenericPlatformAtomics.h"
#include "HIDManager.h"
#include "GamepadDetection.h"
#include "steam/isteaminput.h"
#include "steam/isteamcontroller.h"

FGamepadDetector::FGamepadDetector()
{
	FamilyMap.Add(EGamepadType::PS3_GAMEPAD,
		EGamepadFamily::PS_FAMILY);
	FamilyMap.Add(EGamepadType::PS4_GAMEPAD,
		EGamepadFamily::PS_FAMILY);
	FamilyMap.Add(EGamepadType::XBOX_ONE_GAMEPAD,
		EGamepadFamily::XBOX_FAMILY);
	FamilyMap.Add(EGamepadType::XBOX_360_GAMEPAD,
		EGamepadFamily::XBOX_FAMILY);
	FamilyMap.Add(EGamepadType::SWITCH_GAMEPAD,
		EGamepadFamily::SWITCH_FAMILY);
	FamilyMap.Add(EGamepadType::UNKNOWN_GAMEPAD,
		EGamepadFamily::UNKNOWN_FAMILY);

	SteamGamepadTypeToGamepadTypeMap.Add(
		ESteamInputType::k_ESteamInputType_XBox360Controller,
		EGamepadType::XBOX_360_GAMEPAD);
	SteamGamepadTypeToGamepadTypeMap.Add(
		ESteamInputType::k_ESteamInputType_XBoxOneController,
		EGamepadType::XBOX_ONE_GAMEPAD);
	SteamGamepadTypeToGamepadTypeMap.Add(
		ESteamInputType::k_ESteamInputType_PS3Controller,
		EGamepadType::PS3_GAMEPAD);
	SteamGamepadTypeToGamepadTypeMap.Add(
		ESteamInputType::k_ESteamInputType_PS4Controller,
		EGamepadType::PS4_GAMEPAD);
	SteamGamepadTypeToGamepadTypeMap.Add(
		ESteamInputType::k_ESteamInputType_SwitchJoyConPair,
		EGamepadType::SWITCH_GAMEPAD);
	SteamGamepadTypeToGamepadTypeMap.Add(
		ESteamInputType::k_ESteamInputType_SwitchJoyConSingle,
		EGamepadType::SWITCH_GAMEPAD);
	SteamGamepadTypeToGamepadTypeMap.Add(
		ESteamInputType::k_ESteamInputType_SwitchProController,
		EGamepadType::SWITCH_GAMEPAD);

	SetGamepadType(EGamepadType::UNKNOWN_GAMEPAD);
}

void FGamepadDetector::UpdateGamepadType()
{
	UpdateGamepadTypeSteamBased();
}

void FGamepadDetector::UpdateGamepadTypeSteamBased()
{
	// if steam strategy is selected then use steam, otherwise or if steam is
	// unavailable - use HID based approach
	if (DetectionStrategy == EDetectionStrategy::STEAM_USING_STRATEGY)
	{
		if (SteamInput() != nullptr)
		{
			if (!bIsSteamInputInitialized)
			{
				bIsSteamInputInitialized = SteamInput()->Init();
			}

			if (bIsSteamInputInitialized)
			{
				bool bWasDetected = false;

				ControllerHandle_t ControllerHandles[STEAM_CONTROLLER_MAX_COUNT];
				int32 NumControllers = static_cast<int32>(
					SteamInput()->GetConnectedControllers(ControllerHandles));
				for (int32 i = 0; i < NumControllers; i++)
				{
					ControllerHandle_t ControllerHandle = ControllerHandles[i];
					ESteamInputType InputType =
						SteamInput()->GetInputTypeForHandle(ControllerHandle);

					UE_LOG(LogGamepadDetection, Log, TEXT("Input Type: %d"),
						static_cast<uint32>(InputType));

					auto TypeIt =
						SteamGamepadTypeToGamepadTypeMap.Find(InputType);
					if (TypeIt)
					{
						bWasDetected = true;
						
						SetGamepadType(*TypeIt);
					}
					else
					{
						SetGamepadType(EGamepadType::UNKNOWN_GAMEPAD);
					}
				}

				if (!bWasDetected)
				{
					UE_LOG(LogGamepadDetection, Warning,
						TEXT("The detection failed. No controller was detected"
							 " using Steam"));
				}
			}
			else
			{
				UE_LOG(LogGamepadDetection, Error,
					TEXT("Unable to initialize"" Steam Input"));
			}
		}
		else
		{
			UE_LOG(LogGamepadDetection, Error,
				TEXT("Steam Input is not available (probably Steam Client"
					 " is not initialized) or is not initialized"));
			UE_LOG(LogGamepadDetection, Log,
				TEXT("Proceeding to the traditional algorithm"));

			UpdateGamepadTypeHIDBased();
		}
	}
	else
	{
		UpdateGamepadTypeHIDBased();
	}
}

void FGamepadDetector::UpdateGamepadTypeHIDBased()
{
	if (HIDManager)
	{
		TArray<FHID> HIDs = HIDManager->QueryHIDs();

		for (const auto& HID : HIDs)
		{
			UE_LOG(LogGamepadDetection, Log,
				TEXT("The device's description strings: Hardware ID: %s,"
					 " Vendor ID: %04x, Product ID: %04x"),
				*HID.HardwareID, HID.VendorID, HID.ProductID);

			bool WasSet = FindAndSetGamepad(HID.VendorID,
				HID.ProductID);
			if (WasSet)
			{
				return;
			}
		}

		OnDetectionFailed(HIDs.Num());
	}
	else
	{
		UE_LOG(LogGamepadDetection, Error,
			TEXT("HID manager is not set. Call `SetHIDManager()` first"));
	}
}

void FGamepadDetector::OnDetectionFailed(int NumberOfHIDs)
{
	SetGamepadType(EGamepadType::UNKNOWN_GAMEPAD);

	UE_LOG(LogGamepadDetection, Warning,
		TEXT("The detection failed. Unable to determine type of controller"
			 " by HID-based algorithm: either you're using an unsupported"
			 " controller or no controller was attached. %d is total count"
			 " of connected HIDs"), NumberOfHIDs);
}

bool FGamepadDetector::FindAndSetGamepad(uint32 VendorID, uint32 ProductID)
{
	uint64 Key = 0;

	Key |= VendorID;
	Key <<= 32u;
	Key |= ProductID;

	EGamepadType* Type = ControllersMap.Find(Key);
	if (Type)
	{
		SetGamepadType(*Type);

		return true;
	}

	return false;
}

void FGamepadDetector::SetHIDManager(FHIDManager* NewHIDManager)
{
	HIDManager = NewHIDManager;
}

void FGamepadDetector::SetGamepadType(EGamepadType NewGamepadType)
{
	GamepadType = NewGamepadType;

	EGamepadFamily* NewGamepadFamily = FamilyMap.Find(NewGamepadType);
	if (NewGamepadFamily)
	{
		GamepadFamily = *NewGamepadFamily;
	}
	else
	{
		// in case if a programmer has added a new Gamepad Type but forgot
		// to add the corresponding Family Type
		GamepadFamily = EGamepadFamily::UNKNOWN_FAMILY;
	}
}

int32 FGamepadDetector::GetNumberOfSupportedControllers() const
{
	return ControllersMap.Num();
}

EGamepadType FGamepadDetector::GetGamepadType() const
{
	return GamepadType;
}

EGamepadFamily FGamepadDetector::GetGamepadFamily() const
{
	return GamepadFamily;
}

void FGamepadDetector::SetDetectionStrategy(
	EDetectionStrategy NewDetectionStrategy)
{
	DetectionStrategy = NewDetectionStrategy;
}

EDetectionStrategy FGamepadDetector::GetDetectionStrategy() const
{
	return DetectionStrategy;
}