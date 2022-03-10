// Fill out your copyright notice in the Description page of Project Settings.


#include "SocketPlayerController.h"

#include "NetWorking/Public/Interfaces/IPv4/IPv4Address.h"
#include "TimerManager.h"
#include "Kismet/GamePlayStatics.h"

#include "MsgId.h"
#include "ProjectM_generated.h"


#include "SocketSampleCharacter.h"




void ASocketPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}



}

void ASocketPlayerController::Connect(FString UserID)
{
	if (!IsLocalController())
	{
		return;
	}

	Socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("default"), false);

	FString address = TEXT("127.0.0.1");
	int32 port = 9810;
	FIPv4Address ip;
	FIPv4Address::Parse(address, ip);

	TSharedRef<FInternetAddr> addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	addr->SetIp(ip.Value);
	addr->SetPort(port);

	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Trying to connect.")));

	bConnected = Socket->Connect(*addr);

	if (bConnected)
	{
		GetWorldTimerManager().SetTimer(RecvTimer, this, &ASocketPlayerController::Recv, 0.01, true);
		
		Login(TCHAR_TO_ANSI(*UserID));
	}
}




void ASocketPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void ASocketPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (bConnected)
	{
		//Socket->Shutdown(ESocketShutdownMode::Read);
		Socket->Close();
	}
}



bool ASocketPlayerController::Login(std::string token)
{
	bool r = false;

	flatbuffers::FlatBufferBuilder fbb;
	ProjectM::Actor::C2S_LoginT log;
	log.token = token;
	fbb.Finish(ProjectM::Actor::C2S_Login::Pack(fbb, &log));

	uint32_t head = ((uint32_t)MsgId::C2S_Login << 16) | (4 + fbb.GetSize());
	std::vector<uint8_t> buf(4 + fbb.GetSize());
	memcpy(&buf[0], &head, sizeof(head));
	memcpy(&buf[sizeof(head)], fbb.GetBufferPointer(), fbb.GetSize());
	int32 sentByets = 0;
	r = Socket->Send(buf.data(), buf.size(), sentByets);

	assert(r);

	return true;
}


bool ASocketPlayerController::Move()
{
	ASocketSampleCharacter* NetworkChracter = Cast<ASocketSampleCharacter>(GetPawn());

	if (NetworkChracter && bConnected)
	{
		flatbuffers::FlatBufferBuilder fbb;
		ProjectM::Actor::C2S_SyncLocationT loc;
		loc.actor_id = NetworkChracter->_uid;

		ProjectM::Actor::Transform _trans;
		_trans.mutable_location().mutate_x(NetworkChracter->GetActorLocation().X);
		_trans.mutable_location().mutate_y(NetworkChracter->GetActorLocation().Y);
		_trans.mutable_location().mutate_z(NetworkChracter->GetActorLocation().Z);
		_trans.mutable_rotation().mutate_x(GetControlRotation().Pitch);
		_trans.mutable_rotation().mutate_y(GetControlRotation().Yaw);
		_trans.mutable_rotation().mutate_z(GetControlRotation().Roll);
		_trans.mutable_scale().mutate_x(NetworkChracter->GetActorScale().X);
		_trans.mutable_scale().mutate_y(NetworkChracter->GetActorScale().Y);
		_trans.mutable_scale().mutate_z(NetworkChracter->GetActorScale().Z);
		loc.transform = std::make_unique<ProjectM::Actor::Transform>(_trans);

		fbb.Finish(ProjectM::Actor::C2S_SyncLocation::Pack(fbb, &loc));

		uint32_t head = ((uint32_t)MsgId::C2S_SyncLocation << 16) | (4 + fbb.GetSize());
		std::vector<uint8_t> buf(4 + fbb.GetSize());
		memcpy(&buf[0], &head, sizeof(head));
		memcpy(&buf[sizeof(head)], fbb.GetBufferPointer(), fbb.GetSize());
		int32 sentByets = 0;
		bool r = Socket->Send(buf.data(), buf.size(), sentByets);
		assert(r);

	}
	return true;
}


void ASocketPlayerController::Recv()
{
	ASocketSampleCharacter* NetworkChracter = Cast<ASocketSampleCharacter>(GetPawn());

	if (NetworkChracter && !bConnected)
	{
		return;
	}


	//Binary Array!
	TArray<uint8> ReceivedData;
	uint32_t Head = 0;

	uint32 Size;
	while (Socket->HasPendingData(Size))
	{
		//ReceivedData.Init(0, FMath::Min(Size, 65507u));

		//int32 Read = 0;
		//Socket->Recv(ReceivedData.GetData(), ReceivedData.Num(), Read);


		int32 Read = 0;
		Socket->Recv((uint8*)&Head, 4, Read);

		//UE_LOG(LogTemp, Warning, TEXT("Header %d"), Read);

		////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		MsgId id = (MsgId)HIWORD(Head);
		uint16 sz = LOWORD(Head) - 4;

		//UE_LOG(LogTemp, Warning, TEXT("MesageSize %d"), sz);


		ReceivedData.Init(0, FMath::Min((uint32)sz, 65507u));
		Socket->Recv(ReceivedData.GetData(), sz, Read);


		if (id == MsgId::S2C_Login)
		{
			const ProjectM::Actor::S2C_Login& msg = *flatbuffers::GetRoot<ProjectM::Actor::S2C_Login>(ReceivedData.GetData());
			if (NetworkChracter->_uid == 0)
			{
				NetworkChracter->_uid = msg.actor_id();
				UE_LOG(LogTemp, Warning, TEXT("set S2C_Login actor id %d"), NetworkChracter->_uid);

			}
		}
		else if (id == MsgId::S2C_SpawnActors)
		{
			if (NetworkChracter->_uid == 0)
			{
				continue;
			}

			const ProjectM::Actor::S2C_SpawnActors& msg = *flatbuffers::GetRoot<ProjectM::Actor::S2C_SpawnActors>(ReceivedData.GetData());

			const flatbuffers::Vector<uint64_t>* ids = msg.actor_id();

			for (size_t i = 0; i < ids->Length(); ++i)
			{
				
				UE_LOG(LogTemp, Warning, TEXT("S2C_SpawnActors %d"), (*ids)[i]);
				//New Player
				if (!FindCharacterByUID((*ids)[i]))
				{
					ASocketSampleCharacter* SpawnedCharacter =  GetWorld()->SpawnActor<ASocketSampleCharacter>(SpawnCharacterClass, GetPawn()->GetActorTransform());
					if (SpawnedCharacter)
					{
						UE_LOG(LogClass, Warning, TEXT("Spawn %d"), SpawnedCharacter->_uid);
						SpawnedCharacter->_uid = (*ids)[i];

						const flatbuffers::Vector<const ProjectM::Actor::Transform*>* transforms = msg.transform();
						const ProjectM::Actor::Transform* transform = (*transforms)[i];

						FTransform NewTransform;
						NewTransform.SetLocation(FVector(transform->location().x(), transform->location().y(), transform->location().z()));
						SetControlRotation(FRotator(transform->rotation().x(), transform->rotation().y(), transform->rotation().z()));
						NewTransform.SetScale3D(FVector(transform->scale().x(), transform->scale().y(), transform->scale().z()));

						SpawnedCharacter->SetActorTransform(NewTransform);
					}
				}
			}
		}
		else if (id == MsgId::S2C_SyncLocation)
		{
			const ProjectM::Actor::S2C_SyncLocation& msg = *flatbuffers::GetRoot<ProjectM::Actor::S2C_SyncLocation>(ReceivedData.GetData());
			SyncTransform(msg);
		}
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	if (ReceivedData.Num() <= 0)
	{
		//No Data Received
		return;
	}

	//UE_LOG(LogTemp, Warning, TEXT("Data Bytes Read ~> %d"), ReceivedData.Num());

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//                      Rama's String From Binary Array
	//const FString ReceivedUE4String = StringFromBinaryArray(ReceivedData);
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


	//UE_LOG(LogTemp, Warning, TEXT("As String Data ~> %s"), *ReceivedUE4String);
}

FString ASocketPlayerController::StringFromBinaryArray(const TArray<uint8>& BinaryArray)
{
	//Create a string from a byte array!
	std::string cstr(reinterpret_cast<const char*>(BinaryArray.GetData()), BinaryArray.Num());
	return FString(cstr.c_str());
}


bool ASocketPlayerController::FindCharacterByUID(uint64 UID)
{
	TArray<AActor*> AllCharacters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASocketSampleCharacter::StaticClass(), AllCharacters);
	for (AActor* SelectedCharacter : AllCharacters)
	{
		ASocketSampleCharacter* NetworkCharacter = Cast<ASocketSampleCharacter>(SelectedCharacter);
		if (NetworkCharacter != nullptr)
		{
			//UE_LOG(LogTemp, Warning, TEXT("FindCharacterByUID %d"), NetworkCharacter->_uid);

			if (NetworkCharacter->_uid == UID)
			{
				return true;
			}
		}
	}

	return false;
}


bool ASocketPlayerController::SyncTransform(const ProjectM::Actor::S2C_SyncLocation& msg)
{
	ProjectM::Actor::Transform transform = *msg.transform();
	uint64_t UID = msg.actor_id();

	TArray<AActor*> AllCharacters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ASocketSampleCharacter::StaticClass(), AllCharacters);
	for (AActor* SelectedCharacter : AllCharacters)
	{
		ASocketSampleCharacter* NetworkCharacter = Cast<ASocketSampleCharacter>(SelectedCharacter);
		if (NetworkCharacter != nullptr)
		{
			//UE_LOG(LogTemp, Warning, TEXT("SyncTransform %d"), NetworkCharacter->_uid);

			if (NetworkCharacter->_uid == UID)
			{
				FTransform NewTransform;
				NewTransform.SetLocation(FVector(transform.location().x(), transform.location().y(), transform.location().z()));
				//SetControlRotation(FRotator(transform.rotation().x(), transform.rotation().y(), transform.rotation().z()));
				NewTransform.SetScale3D(FVector(transform.scale().x(), transform.scale().y(), transform.scale().z()));


				NetworkCharacter->SetActorTransform(NewTransform);


				return true;
			}
		}
	}

	return false;
}