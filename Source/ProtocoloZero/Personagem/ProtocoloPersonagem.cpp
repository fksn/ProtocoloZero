// Fill out your copyright notice in the Description page of Project Settings.

#include "ProtocoloPersonagem.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "ProtocoloZero/Weapon/Weapon.h"
#include "ProtocoloZero/ProtocoloComponents/CombatComponent.h"

// Sets default values
AProtocoloPersonagem::AProtocoloPersonagem()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));

	// Mudança 1: Prender na cápsula (RootComponent) no lugar do GetMesh().
	// Isso garante que a câmera nasça no centro do personagem, e não nos pés.
	CameraBoom->SetupAttachment(RootComponent);

	CameraBoom->TargetArmLength = 600.f;
	CameraBoom->bUsePawnControlRotation = true;

	// Mudança 2: Desativar a colisão do braço da câmera com o cenário/personagem.
	CameraBoom->bDoCollisionTest = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// ... (código anterior da FollowCamera)
	FollowCamera->bUsePawnControlRotation = false;

	// --- CONFIGURAÇÃO DA CÂMERA EM PRIMEIRA PESSOA ---
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	// Prende a câmera na malha (Mesh), especificamente no osso da cabeça
	FirstPersonCamera->SetupAttachment(GetMesh(), TEXT("head"));
	FirstPersonCamera->bUsePawnControlRotation = true; // Permite olhar ao redor com o mouse
	FirstPersonCamera->SetActive(false); // Começa desativada, pois o jogo inicia em 3ª pessoa
	// -------------------------------------------------

	bUseControllerRotationYaw = false;

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
}

void AProtocoloPersonagem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AProtocoloPersonagem, OverlappingWeapon, COND_OwnerOnly);
}

void AProtocoloPersonagem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
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

		if (ToggleCameraAction)
		{
			// Usamos ETriggerEvent::Started para que ele troque apenas na hora que a tecla for "Pressionada"
			EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Started, this, &AProtocoloPersonagem::ToggleCamera);
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

void AProtocoloPersonagem::ToggleCamera(const FInputActionValue& Value)
{
	// Inverte o estado atual (Se era false, vira true. Se era true, vira false)
	bIsFirstPerson = !bIsFirstPerson;

	if (bIsFirstPerson)
	{
		// Desliga a Terceira Pessoa e Liga a Primeira Pessoa
		FollowCamera->SetActive(false);
		FirstPersonCamera->SetActive(true);
	}
	else
	{
		// Desliga a Primeira Pessoa e Liga a Terceira Pessoa
		FirstPersonCamera->SetActive(false);
		FollowCamera->SetActive(true);
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
	}
	else
	{
		Crouch();
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