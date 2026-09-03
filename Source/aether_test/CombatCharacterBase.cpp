#include "CombatCharacterBase.h"
#include "CombatPlayerHUDWidget.h"
#include "Animation/AnimSequenceBase.h"
#include "Blueprint/UserWidget.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/ProgressBar.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayAbilitySpec.h"
#include "TimerManager.h"

ACombatCharacterBase::ACombatCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	Attributes = CreateDefaultSubobject<UCombatAttributeSet>(TEXT("Attributes"));
	PlayerHUDClass = UCombatPlayerHUDWidget::StaticClass();
}

void ACombatCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	InitAbilitySystem();
	CreatePlayerHUDIfNeeded();
}

void ACombatCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	CreatePlayerHUDIfNeeded();
}

void ACombatCharacterBase::OnRep_Controller()
{
	Super::OnRep_Controller();
	CreatePlayerHUDIfNeeded();
}

void ACombatCharacterBase::InitAbilitySystem()
{
	if (!ASC)
	{
		return;
	}

	ASC->InitAbilityActorInfo(this, this);

	ASC->GetGameplayAttributeValueChangeDelegate(Attributes->GetHealthAttribute())
		.AddUObject(this, &ACombatCharacterBase::HandleHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(Attributes->GetStaminaAttribute())
		.AddUObject(this, &ACombatCharacterBase::HandleStaminaChanged);

	for (const TSubclassOf<UGameplayEffect>& EffectClass : StartupEffects)
	{
		if (!EffectClass)
		{
			continue;
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
		if (Spec.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}
		ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}
}

bool ACombatCharacterBase::TryPayStamina(float Cost)
{
	if (Cost <= 0.f)
	{
		return true;
	}
	if (!ASC || !Attributes || !StaminaCostEffect)
	{
		// Chua wire GE_StaminaCost thi khong chan combo — chi bao loi ra log
		UE_LOG(LogTemp, Warning, TEXT("TryPayStamina: StaminaCostEffect chua duoc gan tren %s"), *GetName());
		return true;
	}
	if (Attributes->GetStamina() < Cost)
	{
		return false;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(StaminaCostEffect, 1.f, Context);
	if (!Spec.IsValid())
	{
		return true;
	}
	// GE cong them gia tri am = tru stamina
	Spec.Data->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(TEXT("Data.StaminaCost")), -Cost);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return true;
}

FActiveGameplayEffectHandle ACombatCharacterBase::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	if (!ASC || !EffectClass)
	{
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, Level, Context);
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

bool ACombatCharacterBase::ApplyDamageToTarget(ACombatCharacterBase* Target, float Damage, bool bPlayHitReaction)
{
	if (!Target || Target->bIsDead || bIsDead || Damage <= 0.f)
	{
		return false;
	}
	if (!ASC || !DamageEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTarget: DamageEffect chua duoc gan tren %s"), *GetName());
		return false;
	}
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	if (!TargetASC)
	{
		return false;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	Context.AddInstigator(this, this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(DamageEffect, 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	// GE cong gia tri am vao Health = tru mau
	Spec.Data->SetSetByCallerMagnitude(
		FGameplayTag::RequestGameplayTag(TEXT("Data.Damage")), -Damage);
	ASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

	if (!Target->bIsDead && bPlayHitReaction)
	{
		Target->PlayHitReaction();
		Target->OnMeleeHitReceived(this);
	}
	if (IsPlayerControlled())
	{
		RegisterConfirmedHit();
	}
	return true;
}

void ACombatCharacterBase::PlayHitReaction()
{
	if (!bIsDead && HitReactionMontage)
	{
		PlayAnimMontage(HitReactionMontage);
	}
}

void ACombatCharacterBase::ApplyCombatLaunch(float LaunchZ)
{
	if (bIsDead || LaunchZ <= 0.f)
	{
		return;
	}

	if (AController* OwningController = GetController())
	{
		OwningController->StopMovement();
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}

	PlayLaunchReaction();
	LaunchCharacter(FVector(0.f, 0.f, LaunchZ), false, true);
	OnCombatLaunched(LaunchZ);
}

void ACombatCharacterBase::PlayLaunchReaction()
{
	if (LaunchReactionMontage)
	{
		PlayAnimMontage(LaunchReactionMontage);
		return;
	}

	UAnimSequenceBase* ReactionAnimation = LaunchReactionAnimation;
	if (!ReactionAnimation)
	{
		// Existing placed BP_Enemy actors can retain a pre-property-addition null override.
		// Keep the launcher readable until a bespoke authored reaction is assigned in the Blueprint.
		ReactionAnimation = LoadObject<UAnimSequenceBase>(nullptr,
			TEXT("/Game/Characters/Mannequins/Animations/Manny/MM_Fall_Loop.MM_Fall_Loop"));
	}

	if (ReactionAnimation)
	{
		// A dynamic montage lets the project use Manny's existing falling pose now, while retaining
		// a clean authored-montage override slot for the later animation pass.
		if (UAnimMontage* DynamicMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
			ReactionAnimation, TEXT("DefaultSlot"), 0.04f, 0.12f, 1.f, 1))
		{
			PlayAnimMontage(DynamicMontage);
		}
	}
}

void ACombatCharacterBase::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	const float MaxHealth = GetMaxHealth();
	UpdateHealthWidgets(Data.NewValue, MaxHealth);
	OnHealthChanged.Broadcast(Data.NewValue, MaxHealth);
	if (Data.NewValue <= 0.f && !bIsDead)
	{
		bIsDead = true;
		HandleCombatDeath();
		OnCharacterDied.Broadcast(this);
		OnDeath();
	}
}

void ACombatCharacterBase::HandleCombatDeath()
{
	if (bStopMovementOnDeath)
	{
		if (AController* OwningController = GetController())
		{
			OwningController->StopMovement();
		}
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
		// This blocks a Tick-driven child Blueprint from scheduling a fresh MoveTo after death.
		SetActorTickEnabled(false);
	}

	if (bHideHealthWidgetsOnDeath)
	{
		TInlineComponentArray<UWidgetComponent*> WidgetComponents(this);
		GetComponents(WidgetComponents);
		for (UWidgetComponent* WidgetComponent : WidgetComponents)
		{
			if (WidgetComponent)
			{
				WidgetComponent->SetVisibility(false, true);
			}
		}
	}

	if (bRagdollOnDeath)
	{
		if (USkeletalMeshComponent* CharacterMesh = GetMesh())
		{
			CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
			CharacterMesh->SetSimulatePhysics(true);
		}
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	if (DeathDespawnDelay > 0.f)
	{
		SetLifeSpan(DeathDespawnDelay);
	}
}

void ACombatCharacterBase::UpdateHealthWidgets(float NewHealth, float NewMaxHealth)
{
	const float HealthPercent = NewMaxHealth > 0.f
		? FMath::Clamp(NewHealth / NewMaxHealth, 0.f, 1.f)
		: 0.f;

	TInlineComponentArray<UWidgetComponent*> WidgetComponents(this);
	GetComponents(WidgetComponents);
	for (UWidgetComponent* WidgetComponent : WidgetComponents)
	{
		if (!WidgetComponent)
		{
			continue;
		}

		if (UUserWidget* Widget = WidgetComponent->GetUserWidgetObject())
		{
			if (UProgressBar* HealthBar = Cast<UProgressBar>(Widget->GetWidgetFromName(TEXT("HPBar"))))
			{
				HealthBar->SetPercent(HealthPercent);
			}
		}
	}
}

void ACombatCharacterBase::HandleStaminaChanged(const FOnAttributeChangeData& Data)
{
	OnStaminaChanged.Broadcast(Data.NewValue, GetMaxStamina());
}

void ACombatCharacterBase::RegisterConfirmedHit()
{
	++ComboCount;
	OnComboChanged.Broadcast(ComboCount);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ComboResetTimer);
		World->GetTimerManager().SetTimer(ComboResetTimer, this, &ACombatCharacterBase::ResetCombo,
			FMath::Max(0.1f, ComboResetDelay), false);
	}
}

void ACombatCharacterBase::ResetCombo()
{
	if (ComboCount == 0)
	{
		return;
	}
	ComboCount = 0;
	OnComboChanged.Broadcast(ComboCount);
}

void ACombatCharacterBase::CreatePlayerHUDIfNeeded()
{
	if (PlayerHUD || !PlayerHUDClass || !IsPlayerControlled())
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	PlayerHUD = CreateWidget<UCombatPlayerHUDWidget>(PlayerController, PlayerHUDClass);
	if (PlayerHUD)
	{
		PlayerHUD->InitializeForCharacter(this);
		PlayerHUD->AddToViewport();
	}
}

float ACombatCharacterBase::GetHealth() const
{
	return Attributes ? Attributes->GetHealth() : 0.f;
}

float ACombatCharacterBase::GetMaxHealth() const
{
	return Attributes ? Attributes->GetMaxHealth() : 0.f;
}

float ACombatCharacterBase::GetStamina() const
{
	return Attributes ? Attributes->GetStamina() : 0.f;
}

float ACombatCharacterBase::GetMaxStamina() const
{
	return Attributes ? Attributes->GetMaxStamina() : 0.f;
}

bool ACombatCharacterBase::CanDealMeleeDamage(float Cooldown) const
{
	if (Cooldown <= 0.f)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	return !World || World->GetTimeSeconds() >= NextMeleeDamageTime;
}

void ACombatCharacterBase::StartMeleeDamageCooldown(float Cooldown)
{
	if (Cooldown <= 0.f)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		NextMeleeDamageTime = World->GetTimeSeconds() + Cooldown;
	}
}
