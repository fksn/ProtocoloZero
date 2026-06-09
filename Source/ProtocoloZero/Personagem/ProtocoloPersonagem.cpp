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
#include "Components/CapsuleComponent.h"


#include "ProtocoloZero/Weapon/Weapon.h"
#include "ProtocoloZero/ProtocoloComponents/CombatComponent.h"

// Sets default values
AProtocoloPersonagem::AProtocoloPersonagem()
{
	PrimaryActorTick.bCanEverTick = true;

	// --- CÂMERA EM PRIMEIRA PESSOA (FPS) ---
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent()); // Presa na cápsula de colisão (mira perfeita)
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 70.f)); // Altura dos olhos
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetActive(true); // O jogo agora já nasce na visão FPS
	// -------------------------------------------------

	// --- ROTAÇÃO DO PERSONAGEM (Essencial para o Multiplayer) ---
	bUseControllerRotationYaw = true; // Obriga o corpo invisível a girar com o mouse
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false; // Desliga o modo de rotação livre do TPS
	bUseControllerRotationYaw = true;
	// ------------------------------------------------------------

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	// 1. Oculta o corpo mestre da sua própria tela
	GetMesh()->SetOwnerNoSee(true);

	// Obriga a Unreal a calcular a animação do esqueleto invisível
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	GetMesh()->bEnableUpdateRateOptimizations = false;

	// --- CABEÇA ---
	HeadMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(GetMesh());
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetOwnerNoSee(true);
	HeadMesh->bCastHiddenShadow = true;

	// --- TRONCO ---
	TorsoMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TorsoMesh"));
	TorsoMesh->SetupAttachment(GetMesh());
	TorsoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TorsoMesh->SetOwnerNoSee(false);

	// --- BRAÇOS (Você os vê na tela) ---
	ArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmsMesh"));
	ArmsMesh->SetupAttachment(GetMesh());
	ArmsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArmsMesh->SetOwnerNoSee(false);

	// --- PERNAS e PÉS (Você os vê ao olhar para baixo) ---
	LegsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("LegsMesh"));
	LegsMesh->SetupAttachment(GetMesh());
	LegsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LegsMesh->SetOwnerNoSee(false);

	FootsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FootsMesh"));
	FootsMesh->SetupAttachment(GetMesh());
	FootsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FootsMesh->SetOwnerNoSee(false);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
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
		bUseControllerRotationYaw = false;
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
	if (AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
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
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
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

		if (HeadMesh)
		{
			HeadMesh->SetHiddenInGame(true);
			HeadMesh->bCastHiddenShadow = true; // Mantém a sua sombra projetada no chão
		}

		if (TorsoMesh)
		{
			TorsoMesh->SetHiddenInGame(true);
			TorsoMesh->bCastHiddenShadow = true;
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