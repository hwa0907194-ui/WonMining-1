// Copyright Epic Games, Inc. All Rights Reserved.

#include "CXPawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "CXGameState.h"
#include "CXPlayerState.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "CXEngineAudioComponent.h"
#include "CXAutoDownshiftComponent.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY(LogTemplateVehicle);

ACXPawn::ACXPawn()
{
	// construct the front camera boom
	FrontSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Front Spring Arm"));
	FrontSpringArm->SetupAttachment(GetMesh());
	FrontSpringArm->TargetArmLength = 0.0f;
	FrontSpringArm->bDoCollisionTest = false;
	FrontSpringArm->bEnableCameraRotationLag = true;
	FrontSpringArm->CameraRotationLagSpeed = 15.0f;
	FrontSpringArm->SetRelativeLocation(FVector(30.0f, 0.0f, 120.0f));

	FrontCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Front Camera"));
	FrontCamera->SetupAttachment(FrontSpringArm);
	FrontCamera->bAutoActivate = false;

	// construct the back camera boom
	BackSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("Back Spring Arm"));
	BackSpringArm->SetupAttachment(GetMesh());
	BackSpringArm->TargetArmLength = 650.0f;
	BackSpringArm->SocketOffset.Z = 150.0f;
	BackSpringArm->bDoCollisionTest = false;
	BackSpringArm->bInheritPitch = false;
	BackSpringArm->bInheritRoll = false;
	BackSpringArm->bEnableCameraRotationLag = true;
	BackSpringArm->CameraRotationLagSpeed = 2.0f;
	BackSpringArm->CameraLagMaxDistance = 50.0f;

	BackCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("Back Camera"));
	BackCamera->SetupAttachment(BackSpringArm);

	// Configure the car mesh
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName(FName("Vehicle"));
	GetMesh()->SetGenerateOverlapEvents(true);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	bReplicates = true;
	SetReplicateMovement(true);

	// get the Chaos Wheeled movement component
	ChaosVehicleMovement = CastChecked<UChaosWheeledVehicleMovementComponent>(GetVehicleMovement());
	AutoDownshift = CreateDefaultSubobject<UCXAutoDownshiftComponent>(TEXT("Auto Downshift"));
	EngineAudio = CreateDefaultSubobject<UCXEngineAudioComponent>(TEXT("Engine Audio"));
	EngineAudio->SetupAttachment(GetMesh());
	EngineAudio->SetRelativeLocation(FVector(100, 0, 50));

}

void ACXPawn::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (!AttackAction)
		{
			AttackAction = NewObject<UInputAction>(this, TEXT("FallbackAttackAction"));
			AttackAction->ValueType = EInputActionValueType::Boolean;
		}
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &ACXPawn::Fire);
		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				if (AttackMapping) Subsystem->RemoveMappingContext(AttackMapping);
				AttackMapping = NewObject<UInputMappingContext>(this);
				AttackMapping->MapKey(AttackAction, EKeys::LeftMouseButton);
				Subsystem->AddMappingContext(AttackMapping, 1);
				AttackInputSubsystem = Subsystem;
			}
		}
		// steering 
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Triggered, this, &ACXPawn::Steering);
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Completed, this, &ACXPawn::Steering);
		EnhancedInputComponent->BindAction(SteeringAction, ETriggerEvent::Canceled, this, &ACXPawn::Steering);

		// throttle 
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Triggered, this, &ACXPawn::Throttle);
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Completed, this, &ACXPawn::Throttle);
		EnhancedInputComponent->BindAction(ThrottleAction, ETriggerEvent::Canceled, this, &ACXPawn::Throttle);

		// break 
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Triggered, this, &ACXPawn::Brake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Started, this, &ACXPawn::StartBrake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Completed, this, &ACXPawn::StopBrake);
		EnhancedInputComponent->BindAction(BrakeAction, ETriggerEvent::Canceled, this, &ACXPawn::StopBrake);

		// handbrake 
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Started, this, &ACXPawn::StartHandbrake);
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Completed, this, &ACXPawn::StopHandbrake);
		EnhancedInputComponent->BindAction(HandbrakeAction, ETriggerEvent::Canceled, this, &ACXPawn::StopHandbrake);

		// look around 
		EnhancedInputComponent->BindAction(LookAroundAction, ETriggerEvent::Triggered, this, &ACXPawn::LookAround);

		// toggle camera 
		EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &ACXPawn::ToggleCamera);

		// reset the vehicle 
		EnhancedInputComponent->BindAction(ResetVehicleAction, ETriggerEvent::Started, this, &ACXPawn::ResetVehicle);
	}
	else
	{
		UE_LOG(LogTemplateVehicle, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void ACXPawn::Tick(float Delta)
{
	Super::Tick(Delta);
	UpdateEngineAudio(Delta);
	if (HasAuthority() || IsLocallyControlled())
	{
		const bool bRaceLocked = !IsRacePlaying();
		if (bRaceLocked != bWasRaceLocked) ChaosVehicleMovement->SetHandbrakeInput(bRaceLocked);
		bWasRaceLocked = bRaceLocked;
		if (bRaceLocked)
		{
			ChaosVehicleMovement->SetThrottleInput(0);
			ChaosVehicleMovement->SetSteeringInput(0);
		}
		const ACXPlayerState* Stats = GetPlayerState<ACXPlayerState>();
		const float SpeedLimit = (BaseMaxSpeedKmh + (Stats ? Stats->GetBonusMaxSpeed() : 0.0f)) / 0.036f;
		const FVector Velocity = GetMesh()->GetPhysicsLinearVelocity();
		if (Velocity.SizeSquared2D() > FMath::Square(SpeedLimit))
		{
			const FVector Limited = Velocity.GetSafeNormal2D() * SpeedLimit;
			GetMesh()->SetPhysicsLinearVelocity(FVector(Limited.X, Limited.Y, Velocity.Z));
		}
	}

	// add some angular damping if the vehicle is in midair
	bool bMovingOnGround = ChaosVehicleMovement->IsMovingOnGround();
	const float AngularDamping = bMovingOnGround ? 0.0f : 3.0f;
	if (GetMesh()->GetAngularDamping() != AngularDamping) GetMesh()->SetAngularDamping(AngularDamping);

	// realign the camera yaw to face front
	if (!IsLocallyControlled()) return;
	float CameraYaw = BackSpringArm->GetRelativeRotation().Yaw;
	CameraYaw = FMath::FInterpTo(CameraYaw, 0.0f, Delta, 1.0f);

	BackSpringArm->SetRelativeRotation(FRotator(0.0f, CameraYaw, 0.0f));
}

void ACXPawn::Steering(const FInputActionValue& Value)
{
	// get the input magnitude for steering
	float SteeringValue = IsRacePlaying() ? Value.Get<float>() : 0.0f;

	// add the input
	ChaosVehicleMovement->SetSteeringInput(SteeringValue);
}

void ACXPawn::Throttle(const FInputActionValue& Value)
{
	// get the input magnitude for the throttle
	float ThrottleValue = IsRacePlaying() ? Value.Get<float>() : 0.0f;

	// add the input
	ChaosVehicleMovement->SetThrottleInput(ThrottleValue);
}

void ACXPawn::Brake(const FInputActionValue& Value)
{
	// get the input magnitude for the brakes
	float BreakValue = Value.Get<float>();

	// add the input
	ChaosVehicleMovement->SetBrakeInput(BreakValue);
}

void ACXPawn::StartBrake(const FInputActionValue& Value)
{
	// call the Blueprint hook for the break lights
	BrakeLights(true);
}

void ACXPawn::StopBrake(const FInputActionValue& Value)
{
	// call the Blueprint hook for the break lights
	BrakeLights(false);

	// reset brake input to zero
	ChaosVehicleMovement->SetBrakeInput(0.0f);
}

void ACXPawn::StartHandbrake(const FInputActionValue& Value)
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(true);

	// call the Blueprint hook for the break lights
	BrakeLights(true);
}

void ACXPawn::StopHandbrake(const FInputActionValue& Value)
{
	// add the input
	ChaosVehicleMovement->SetHandbrakeInput(!IsRacePlaying());

	// call the Blueprint hook for the break lights
	BrakeLights(false);
}

void ACXPawn::LookAround(const FInputActionValue& Value)
{
	// get the flat angle value for the input 
	float LookValue = Value.Get<float>();

	// add the input
	BackSpringArm->AddLocalRotation(FRotator(0.0f, LookValue, 0.0f));
}

void ACXPawn::ToggleCamera(const FInputActionValue& Value)
{
	// toggle the active camera flag
	bFrontCameraActive = !bFrontCameraActive;

	FrontCamera->SetActive(bFrontCameraActive);
	BackCamera->SetActive(!bFrontCameraActive);
}

void ACXPawn::ResetVehicle(const FInputActionValue& Value)
{
	if (!IsRacePlaying()) return;
	// reset to a location slightly above our current one
	FVector ResetLocation = GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

	// reset to our yaw. Ignore pitch and roll
	FRotator ResetRotation = GetActorRotation();
	ResetRotation.Pitch = 0.0f;
	ResetRotation.Roll = 0.0f;
	
	// teleport the actor to the reset spot and reset physics
	SetActorTransform(FTransform(ResetRotation, ResetLocation, FVector::OneVector), false, nullptr, ETeleportType::TeleportPhysics);

	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);

	UE_LOG(LogTemplateVehicle, Verbose, TEXT("Reset Vehicle"));
}

void ACXPawn::EndPlay(const EEndPlayReason::Type Reason)
{
	if (EngineAudio) EngineAudio->Stop();
	if (AttackInputSubsystem.IsValid() && AttackMapping)
		AttackInputSubsystem->RemoveMappingContext(AttackMapping);
	Super::EndPlay(Reason);
}

void ACXPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACXPawn, EngineAudioRPM, COND_SkipOwner);
}

void ACXPawn::UpdateEngineAudio(float DeltaSeconds)
{
	EngineAudioElapsed += DeltaSeconds;
	if (EngineAudioElapsed < 0.1f) return;
	EngineAudioElapsed = 0.0f;
	const float IdleRPM = FMath::Clamp(ChaosVehicleMovement->EngineSetup.EngineIdleRPM, 400.0f, 20000.0f);
	const float MaxRPM = FMath::Max(IdleRPM + 1, ChaosVehicleMovement->EngineSetup.MaxRPM);
	float RPM = EngineAudioRPM;
	if (HasAuthority() || IsLocallyControlled())
	{
		const float PhysicsRPM = ChaosVehicleMovement->GetEngineRotationSpeed();
		RPM = FMath::IsFinite(PhysicsRPM) ? FMath::Clamp(PhysicsRPM, IdleRPM, 20000.0f) : IdleRPM;
		if (HasAuthority()) EngineAudioRPM = static_cast<uint16>(FMath::RoundToInt(RPM));
	}
	if (GetNetMode() == NM_DedicatedServer) return;
	// Remote cars use authoritative RPM. Their load is approximated from RPM;
	// the locally driven car responds to the actual throttle input as well.
	const float Load = IsLocallyControlled() ? FMath::Abs(ChaosVehicleMovement->GetThrottleInput())
		: FMath::Clamp((RPM - IdleRPM) / (MaxRPM - IdleRPM), 0.0f, 1.0f);
	EngineAudio->SetEngineParameters(FMath::Max(RPM, IdleRPM), Load);
}

bool ACXPawn::IsRacePlaying() const
{
	const ACXGameState* GS = GetWorld() ? GetWorld()->GetGameState<ACXGameState>() : nullptr;
	return GS && GS->GetPhase() == ECXMatchPhase::Playing;
}

void ACXPawn::MulticastResetForRound_Implementation(const FTransform& SpawnTransform)
{
	SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	GetMesh()->SetPhysicsLinearVelocity(FVector::ZeroVector);
	GetMesh()->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	ChaosVehicleMovement->SetThrottleInput(0);
	ChaosVehicleMovement->SetSteeringInput(0);
	ChaosVehicleMovement->SetBrakeInput(0);
	ChaosVehicleMovement->SetHandbrakeInput(true);
	bWasRaceLocked = true;
	LastAttackTime = -100;
	if (HasAuthority()) ForceNetUpdate();
}

bool ACXPawn::CanFire() const
{
	return IsRacePlaying() && GetController() && GetWorld()->GetTimeSeconds() - LastAttackTime >= AttackCooldown;
}

void ACXPawn::Fire()
{
	if ((HasAuthority() || IsLocallyControlled()) && CanFire()) ServerFire();
}

void ACXPawn::ServerFire_Implementation()
{
	if (!CanFire()) return;
	LastAttackTime = GetWorld()->GetTimeSeconds();
	// Origin and aim are derived from the authoritative car, never client input.
	const FVector Start = GetActorTransform().TransformPosition(AttackBumperOffset);
	const FVector End = Start + GetActorForwardVector() * AttackRange;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CXAttack), false, this);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	ACXPawn* Victim = bHit ? Cast<ACXPawn>(Hit.GetActor()) : nullptr;
	if (Victim && Victim != this) UGameplayStatics::ApplyDamage(Victim, 5.0f, GetController(), this, UDamageType::StaticClass());
	if (ACXGameState* GS = GetWorld()->GetGameState<ACXGameState>()) GS->MulticastAttack(Start, bHit ? Hit.ImpactPoint : End, Victim != nullptr);
}

float ACXPawn::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	ACXPawn* Attacker = Cast<ACXPawn>(DamageCauser);
	if (!HasAuthority() || !IsRacePlaying() || DamageAmount < 5 || !IsValid(Attacker) || Attacker == this || EventInstigator != Attacker->GetController()) return 0;
	ACXPlayerState* VictimStats = GetPlayerState<ACXPlayerState>();
	ACXPlayerState* AttackerStats = Attacker->GetPlayerState<ACXPlayerState>();
	return VictimStats && VictimStats->TransferCoinsTo(AttackerStats, 5) ? 5.0f : 0.0f;
}
