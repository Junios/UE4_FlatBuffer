// Copyright Epic Games, Inc. All Rights Reserved.

#include "SocketSampleGameMode.h"
#include "SocketSampleCharacter.h"
#include "UObject/ConstructorHelpers.h"

ASocketSampleGameMode::ASocketSampleGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
