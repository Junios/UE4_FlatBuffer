// Copyright Epic Games, Inc. All Rights Reserved.

#include "SocketSampleCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

#include "NetWorking/Public/Interfaces/IPv4/IPv4Address.h"
#include "TimerManager.h"




//////////////////////////////////////////////////////////////////////////
// ASocketSampleCharacter

ASocketSampleCharacter::ASocketSampleCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void ASocketSampleCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &ASocketSampleCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ASocketSampleCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ASocketSampleCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ASocketSampleCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &ASocketSampleCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &ASocketSampleCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &ASocketSampleCharacter::OnResetVR);
}


void ASocketSampleCharacter::OnResetVR()
{
	// If SocketSample is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in SocketSample.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void ASocketSampleCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void ASocketSampleCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void ASocketSampleCharacter::BeginPlay()
{
	Super::BeginPlay();
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
		GetWorldTimerManager().SetTimer(RecvTimer, this, &ASocketSampleCharacter::Recv, 0.01, true);
		Login("a1");
	}
}

void ASocketSampleCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Move();
}

void ASocketSampleCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ASocketSampleCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void ASocketSampleCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void ASocketSampleCharacter::MoveRight(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}


struct Actor
{

};

class World
{
public:
	std::unordered_map<uint64_t, Actor> _objMap;
};


bool ASocketSampleCharacter::Login(std::string token)
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

bool ASocketSampleCharacter::Move()
{
	flatbuffers::FlatBufferBuilder fbb;
	ProjectM::Actor::C2S_SyncLocationT loc;
	loc.actor_id = _uid;
	_trans.mutable_location().mutate_x(GetActorLocation().X);
	_trans.mutable_location().mutate_y(GetActorLocation().Y);
	_trans.mutable_location().mutate_z(GetActorLocation().Z);
	_trans.mutable_rotation().mutate_x(GetControlRotation().Pitch);
	_trans.mutable_rotation().mutate_y(GetControlRotation().Yaw);
	_trans.mutable_rotation().mutate_z(GetControlRotation().Roll);
	_trans.mutable_scale().mutate_x(GetActorScale().X);
	_trans.mutable_scale().mutate_y(GetActorScale().Y);
	_trans.mutable_scale().mutate_z(GetActorScale().Z);
	loc.transform = std::make_unique<ProjectM::Actor::Transform>(_trans);

	fbb.Finish(ProjectM::Actor::C2S_SyncLocation::Pack(fbb, &loc));

	uint32_t head = ((uint32_t)MsgId::C2S_SyncLocation << 16) | (4 + fbb.GetSize());
	std::vector<uint8_t> buf(4 + fbb.GetSize());
	memcpy(&buf[0], &head, sizeof(head));
	memcpy(&buf[sizeof(head)], fbb.GetBufferPointer(), fbb.GetSize());
	int32 sentByets = 0;
	bool r = Socket->Send(buf.data(), buf.size(), sentByets);
	assert(r);

	return true;
}

//uint32_t head;
//int len = CAsyncSocket::Receive(&head, sizeof(head));
//if (len == SOCKET_ERROR)
//{
//	int e = WSAGetLastError();
//	if (e == WSAEWOULDBLOCK) break;
//}
//assert(len == sizeof(head));
//


void ASocketSampleCharacter::Recv()
{
	//~~~~~~~~~~~~~
	if (!bConnected) return;
	//~~~~~~~~~~~~~


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

		UE_LOG(LogTemp, Warning, TEXT("Header %d"), Read);

		////~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		MsgId id = (MsgId)HIWORD(Head);
		uint16 sz = LOWORD(Head) - 4;

		UE_LOG(LogTemp, Warning, TEXT("MesageSize %d"), sz);


		ReceivedData.Init(0, FMath::Min((uint32)sz, 65507u));
		Socket->Recv(ReceivedData.GetData(), sz, Read);


		if (id == MsgId::S2C_Login)
		{
			const ProjectM::Actor::S2C_Login& msg = *flatbuffers::GetRoot<ProjectM::Actor::S2C_Login>(ReceivedData.GetData());
			_uid = msg.actor_id();
		}
		else if (id == MsgId::S2C_SpawnActors)
		{
			const ProjectM::Actor::S2C_SpawnActors& msg = *flatbuffers::GetRoot<ProjectM::Actor::S2C_SpawnActors>(ReceivedData.GetData());
		}
		else if (id == MsgId::S2C_SyncLocation)
		{
			const ProjectM::Actor::S2C_SyncLocation& msg = *flatbuffers::GetRoot<ProjectM::Actor::S2C_SyncLocation>(ReceivedData.GetData());
			UE_LOG(LogTemp, Warning, TEXT("S2C_SyncLocation"));

			if (_uid != msg.actor_id())
			{
				//Actor& act = _world._objMap[msg.actor_id()];
				//act._uid = msg.actor_id();
				//act._trans = *msg.transform();
			}
		}
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	}
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	if (ReceivedData.Num() <= 0)
	{
		//No Data Received
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("Data Bytes Read ~> %d"), ReceivedData.Num());

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//                      Rama's String From Binary Array
	const FString ReceivedUE4String = StringFromBinaryArray(ReceivedData);
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


	UE_LOG(LogTemp, Warning, TEXT("As String Data ~> %s"), *ReceivedUE4String);
}

FString ASocketSampleCharacter::StringFromBinaryArray(const TArray<uint8>& BinaryArray)
{
	//Create a string from a byte array!
	std::string cstr(reinterpret_cast<const char*>(BinaryArray.GetData()), BinaryArray.Num());
	return FString(cstr.c_str());
}