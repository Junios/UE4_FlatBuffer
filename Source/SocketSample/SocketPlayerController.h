// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"


#include "Networking.h"
#include "Sockets.h"
#include "SocketSubsystem.h"


#include "ProjectM_generated.h"

#include <memory>
#include <unordered_map>

#include "SocketPlayerController.generated.h"


class ASocketSampleCharacter;

UCLASS()
class SOCKETSAMPLE_API ASocketPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Network")
	void Connect(FString UserID);

	virtual void Tick(float DeltaSeconds) override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:



	bool Login(std::string token);

	bool Move();


	void Recv();

	FString StringFromBinaryArray(const TArray<uint8>& BinaryArray);


	FTimerHandle RecvTimer;

	FSocket* Socket;

	bool bConnected;


	bool FindCharacterByUID(const uint64 UID);

	bool SyncTransform(const ProjectM::Actor::S2C_SyncLocation& msg);

	UPROPERTY(EditAnywhere, Category = "Data", BlueprintReadWrite)
	TSubclassOf<ASocketSampleCharacter> SpawnCharacterClass;
};
