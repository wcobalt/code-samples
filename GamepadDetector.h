// Copyright Flying Wild Hog. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CoreTypes.h"
#include <regex>
#include "HIDManager.h"
#include "steam/isteaminput.h"
#include "GamepadDetector.generated.h"

/**
* Enum which describes what kind of gamepad the player is using.
* Is a blueprint type
*/
UENUM(BlueprintType)
enum class EGamepadType : uint8
{
	PS4_GAMEPAD,
	PS3_GAMEPAD,
	XBOX_ONE_GAMEPAD,
	XBOX_360_GAMEPAD,
	SWITCH_GAMEPAD,
	UNKNOWN_GAMEPAD
};

/**
* Enum which describes what is the family of gamepad the player is using.
* Is a blueprint type
*
* The gamepads and the families are related to each other in the following way:
* - `PS4_GAMEPAD` -> `PS_FAMILY`
* - `PS3_GAMEPAD` -> `PS_FAMILY`
* - `XBOX_ONE_GAMEPAD` -> `XBOX_FAMILY`
* - `XBOX_360_GAMEPAD` -> `XBOX_FAMILY`
* - `SWITCH_GAMEPAD` -> `SWITCH_FAMILY`
* - `UNKNOWN_GAMEPAD` -> `UNKNOWN_FAMILY`
*/
UENUM(BlueprintType)
enum class EGamepadFamily : uint8
{
	PS_FAMILY,
	XBOX_FAMILY,
	SWITCH_FAMILY,
	UNKNOWN_FAMILY
};

/**
* Enum which describes what strategy gamepad detector must use
* 
* 1. `STEAM_USING_STRATEGY` - The detection is based on the Steam-algorithm and
* the tradional one
* 2. `NO_STEAM_STRATEGY` - The detection is based only on the tradional
* algorithm
* 
* See `UpdateGamepadType()` for detailed description of each stage.
* @see UpdateGamepadType()
*/
UENUM()
enum class EDetectionStrategy : uint8
{
	STEAM_USING_STRATEGY,
	NO_STEAM_STRATEGY
};

/**
* Class which provides means to determine the type and the family of gamepad
* the player is currently using
*/
class GAMEPADDETECTION_API FGamepadDetector
{
public:
	/**
	* The default constructor
	*/
	FGamepadDetector();

	/**
	* Determines the type and the family of gamepad the player is currently
	* using
	*
	* If the current gamepad detection strategy is
	* `EDetectionStrategy::STEAM_USING_STRATEGY` then the detector first tries
	* to use steam services to determine the type of gamepad. If the services
	* are unavailable then the detector proceeds to the traditional algorithm.
	* If the `EDetectionStrategy::NO_STEAM_STRATEGY` is set as the current
	* strategy the traditional algorithm is the first and the only used
	* algorithm.
	* The traditional algorithm does not run if the HID manager is not set.
	* 
	* The traditional algorithm is based on list of connected HID devices
	* (which is retrieved from hid.dll) which. During the execution of the
	* algorithm the method retrieves list of connected HIDs and tries to find
	* each of connected HIDs in the list of supported controllers (which is a
	* precise copy of this database
	* https://support.steampowered.com/kb/5199-TOKV-4426/supported-controller-database
	* (as is on 5th May of 2021)) whereas the search is performed based on
	* Vendor and Product IDs of the controllers.
	*
	* The Steam-based algorithm is based (as it's clear from the name) on Steam
	* Client API. It uses the Steam Input system through which a list of
	* connected controllers is retrieved after what the type of the first
	* controller is set as the current type (family is set accordingly).
	* 
	* If no HID has been determined as a controller during by the traditional
	* algorithm or no controllers were found by the steam-based one,
	* `EGamepadType::UNKNOWN_GAMEPAD` and `EGamepadFamily::UNKNOWN_FAMILY` are
	* used as current type and family values. If there were multiple HIDs
	* determined as controllers (no matter during which stage - the first,
	* the second or the third one) then it is undefined what controller is
	* used to set current type and family values.
	*
	* @see EGamepadFamily, EGamepadType
	*/
	virtual void UpdateGamepadType();

	/**
	* Returns last detected type of gamepad
	*
	* @return Last detected type of gamepad
	* @see EGamepadType
	*/
	virtual EGamepadType GetGamepadType() const;

	/**
	* Returns last detected family of gamepad
	*
	* @return Last detected family of gamepad
	* @see EGamepadFamily
	*/
	virtual EGamepadFamily GetGamepadFamily() const;

	/**
	* Sets the gamepad detection strategy
	* 
	* By default `EDetectionStrategy::STEAM_USING_STRATEGY` is set as current
	* gamepad detection strategy.
	* 
	* @param NewDetectionStrategy A gamepad detection strategy to be set as
	* current one
	* @see EDetectionStrategy
	*/
	void SetDetectionStrategy(EDetectionStrategy NewDetectionStrategy);

	/**
	* Returns currently used gamepad detection strategy
	* 
	* @return The gamepad detection strategy currently in use
	* @see EDetectionStrategy
	*/
	EDetectionStrategy GetDetectionStrategy() const;

	/**
	* Adds support of a controller through manual mapping of VID and PID to
	* the type
	* 
	* @param VendorID Vendor id of the device
	* @param ProductID Product id of the device
	* @param Type Type of the device
	*/
	inline void AddControllerSupport(uint32 VendorID, uint32 ProductID,
		EGamepadType Type)
	{
		uint64 Key = 0;

		Key |= VendorID;
		Key <<= 32u;
		Key |= ProductID;

		ControllersMap.Add(Key, Type);
	}

	/**
	* Returns count of controllers which were added through
	* `AddControllerSupport()`
	* 
	* Note: count of `AddControllerSupport()` calls and value returned by this
	* method may be different
	* 
	* @return Count of supported controllers
	*/
	int32 GetNumberOfSupportedControllers() const;

	/**
	* Sets HID manager
	* 
	* It's necessary to set HID manager in order for the traditional
	* algorithm to work
	* 
	* @param NewHIDManager HID manager to use
	* @see UpdateGamepadType()
	*/
	void SetHIDManager(FHIDManager* NewHIDManager);

	/**
	* Default destructor
	*/
	virtual ~FGamepadDetector() = default;

private:
	/**
	* Sets the current type of gamepad. Automatically sets the current family
	* of gamepad to the according value
	*
	* @param NewGamepadType a gamepad type to be set as current
	* @see EGamepadFamily, EGamepadType
	*/
	void SetGamepadType(EGamepadType NewGamepadType);

	/**
	* Searches `ControllersMap` for an entry with matching Vendor ID and
	* Product ID and if succeeds sets according type and family
	* 
	* If `ControllersMap` contains an entry with specified Product ID and
	* Vendor ID sets the type from that entry and the according to that type
	* family as current gamepad type and family values
	* 
	* @param VendorID Vendor ID of a device
	* @param ProductID Product ID of a device
	* @return `true` if the entry was found, `false` - otherwise
	*/
	bool FindAndSetGamepad(uint32 VendorID, uint32 ProductID);

	/**
	* Executes the traditional HID-based algorithm of the gamepad detection
	* 
	* Does nothing if the HID manager is not set.
	* See `UpdateGamepadType()` for detailed description of the first stage
	* 
	* @see UpdateGamepadType
	*/
	void UpdateGamepadTypeHIDBased();

	/**
	* Executes the Steam-based algorithm of the gamepad detection
	*
	* See `UpdateGamepadType()` for detailed description of the second stage
	*
	* @param HIDs A list of HIDs obtained during the first stage. Is a list of
	* currently connected HIDs (no the raw ones)
	* @see UpdateGamepadType
	*/
	void UpdateGamepadTypeSteamBased();

	/**
	* Prints a message in the log and sets default values as current type and
	* family values
	* 
	* `Default values` means `EGamepadType::UNKNOWN_GAMEPAD` and
	* `EGamepadFamily::UNKNOWN_FAMILY`.
	* Supposed to be called when the gamepad detection based on HIDs fails
	* 
	* @param NumberOfHIDs Number of connected HIDs (not the raw ones) to be
	* printed along with other information in the log
	*/
	void OnDetectionFailed(int NumberOfHIDs);

	//Indicates whether the Steam Input was initialized or not
	bool bIsSteamInputInitialized = false;

	/**
	* Is used to contain the supported controllers and their Vendor and
	* Product IDs
	* The key is as follows 0x0000VVVV0000PPPP, where VVVV is Vendor ID and
	* PPPP is Product ID
	*/
	TMap<uint64, EGamepadType> ControllersMap;

	//Is used to define the relation between gamepad types and gamepad families
	TMap<EGamepadType, EGamepadFamily> FamilyMap;

	//Is used to define the relation between Steam gamepad types and
	// gamepad types in terms of EGamepadType
	TMap<ESteamInputType, EGamepadType> SteamGamepadTypeToGamepadTypeMap;
	
	//Contains family of gamepad the player is using
	EGamepadFamily GamepadFamily;

	//Contains type of gamepad the player is using
	EGamepadType GamepadType;

	//Manager of HIDs
	FHIDManager* HIDManager;

	//Currently selected gamepad detection strategy
	EDetectionStrategy DetectionStrategy =
		EDetectionStrategy::STEAM_USING_STRATEGY;
};
