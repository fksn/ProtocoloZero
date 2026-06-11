// Fill out your copyright notice in the Description page of Project Settings.

#include "ProtocoloPersonagem.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Engine/Engine.h"
#include "Components/CapsuleComponent.h"


#include "ProtocoloZero/Weapon/Weapon.h"
#include "ProtocoloZero/ProtocoloComponents/CombatComponent.h"

// Sets default values
AProtocoloPersonagem::AProtocoloPersonagem()
{
	PrimaryActorTick.bCanEverTick = true;

	// --- CÂMERA EM PRIMEIRA PESSOA (FPS) ---
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	GetMesh()->SetOwnerNoSee(false);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 900.f);
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;
}

void AProtocoloPersonagem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AProtocoloPersonagem, OverlappingWeapon, COND_OwnerOnly);
}

void AProtocoloPersonagem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AimOffset(DeltaTime);
	if (GEngine && Combat)
	{
		// Cria o texto baseado no status da mira
		FString StatusTexto = IsAiming() ? TEXT("Mirando: SIM") : TEXT("Mirando: NAO");

		// Escolhe a cor (Verde para mirando, Vermelho para nao mirando)
		FColor CorTexto = IsAiming() ? FColor::Green : FColor::Red;

		// Imprime na tela. O "1" no primeiro parâmetro é a Key que impede a tela de encher de mensagens repetidas.
		// O 0.0f é o tempo de tela (como atualiza todo frame, 0 é suficiente).
		GEngine->AddOnScreenDebugMessage(1, 0.0f, CorTexto, StatusTexto);
	}
}

// Called when the game starts or when spawned
void AProtocoloPersonagem::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Verifica se a variável DefaultMappingContext foi definida na Blueprint
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void AProtocoloPersonagem::Jump()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
	}
}

void AProtocoloPersonagem::AimOffset(float DeltaTime)
{
	if (Combat && Combat->EquippedWeapon == nullptr) return;
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	float Speed = Velocity.Size();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	if (Speed == 0.f && !bIsInAir) // standing still, not jumping
	{
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		TurnInPlace(DeltaTime);
	}
	if (Speed > 0.f || bIsInAir) // running, or jumping
	{
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}

	AO_Pitch = GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		// map pitch from [270, 360) to [-90, 0)
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void AProtocoloPersonagem::TurnInPlace(float DeltaTime)
{
	GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, *UEnum::GetValueAsString(TurningInPlace));
	if (AO_Yaw > 45.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, *UEnum::GetValueAsString(TurningInPlace));
	}
	else if (AO_Yaw < -45.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, *UEnum::GetValueAsString(TurningInPlace));
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

// Called to bind functionality to input
void AProtocoloPersonagem::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Cast para o novo sistema Enhanced Input Component
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jump (Ação do tipo bool - Verdadeiro/Falso)
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AProtocoloPersonagem::Jump);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		}

		// Move (Ação do tipo Axis2D - Vetor 2D)
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProtocoloPersonagem::Move);
		}

		// Look (Ação do tipo Axis2D - Vetor 2D)
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProtocoloPersonagem::Look);
		}
		if (EquipAction)
		{
			EnhancedInputComponent->BindAction(EquipAction, ETriggerEvent::Started, this, &AProtocoloPersonagem::EquipButtonPressed);
		}

		if (CrouchAction)
		{
			EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AProtocoloPersonagem::CrouchButtonPressed);
		}

		if (AimAction)
		{
			// IE_Pressed vira ETriggerEvent::Started
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Started, this, &AProtocoloPersonagem::AimButtonPressed);

			// IE_Released vira ETriggerEvent::Completed
			EnhancedInputComponent->BindAction(AimAction, ETriggerEvent::Completed, this, &AProtocoloPersonagem::AimButtonReleased);
		}
	}
}

void AProtocoloPersonagem::PostInitializeComponents()
{
		Super::PostInitializeComponents();
		if (Combat)
		{
			Combat->Personagem = this;
		}
}

bool AProtocoloPersonagem::IsWeaponEquipped()
{
	return (Combat && Combat->EquippedWeapon);
}

void AProtocoloPersonagem::Move(const FInputActionValue& Value)
{
	// O Enhanced Input nos entrega um Vector2D onde X é Direita/Esquerda (A/D) e Y é Frente/Trás (W/S)
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// Pega a rotação atual da câmera, mas isola apenas o eixo Yaw (para andar reto no chão)
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// Calcula qual direção é "Frente" baseado na rotação da câmera
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		// Calcula qual direção é "Direita" baseado na rotação da câmera
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// Adiciona o movimento (Y = Eixo Frente/Trás | X = Eixo Direita/Esquerda)
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AProtocoloPersonagem::Look(const FInputActionValue& Value)
{
	// Aqui também recebemos um Vector2D onde X é o movimento horizontal do mouse e Y é o vertical
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// Adiciona as rotações ao controlador do personagem
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void AProtocoloPersonagem::EquipButtonPressed(const FInputActionValue& Value)
{
	if (Combat)
	{
		if (HasAuthority())
		{
			Combat->EquipWeapon(OverlappingWeapon);
		}
		else
		{
			ServerEquipButtonPressed();
		}
	}
}

void AProtocoloPersonagem::CrouchButtonPressed(const FInputActionValue& Value)
{
	if (bIsCrouched)
	{
		UnCrouch();

		// 1. Imprime na tela do jogo (Cor Azul Claro, duração de 3 segundos)
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("Ação: UnCrouch() chamado!"));
		}

		// 2. Imprime no Console / Output Log (Aparece em amarelo)
		UE_LOG(LogTemp, Warning, TEXT("Ação: UnCrouch() chamado!"));
	}
	else
	{
		Crouch();

		// 1. Imprime na tela do jogo
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("Ação: Crouch() chamado!"));
		}

		// 2. Imprime no Console / Output Log
		UE_LOG(LogTemp, Warning, TEXT("Ação: Crouch() chamado!"));
	}
}

void AProtocoloPersonagem::AimButtonPressed()
{
	if (Combat)
	{
		Combat->SetAiming(true);
	}
}

void AProtocoloPersonagem::AimButtonReleased()
{
	if (Combat)
	{
		Combat->SetAiming(false);
	}
}

bool AProtocoloPersonagem::IsAiming()
{
	return (Combat && Combat->bAiming);
}

void AProtocoloPersonagem::ServerEquipButtonPressed_Implementation()
{
	if (Combat)
	{
		Combat->EquipWeapon(OverlappingWeapon);
	}
}

void AProtocoloPersonagem::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(false);
	}
	OverlappingWeapon = Weapon;
	if (IsLocallyControlled())
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

void AProtocoloPersonagem::OnRep_OverlappingWeapon(AWeapon* LastWeapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

AWeapon* AProtocoloPersonagem::GetEquippedWeapon()
{
	if (Combat == nullptr) return nullptr;
	return Combat->EquippedWeapon;
}