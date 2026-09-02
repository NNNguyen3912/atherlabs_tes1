#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_MeleeHitbox.generated.h"

class UGameplayEffect;
class ACombatCharacterBase;

/**
 * Hitbox don danh: dat dai notify quanh frame cham cua montage.
 * Sweep sphere theo socket giua 2 tick — moi nan nhan chi dinh 1 lan cho moi lan phat.
 * LUU Y: notify object DUNG CHUNG giua moi character phat cung montage,
 * nen state per-activation phai key theo MeshComp (khong duoc de member tran lan).
 */
UCLASS(meta = (DisplayName = "Melee Hitbox"))
class AETHER_TEST_API UANS_MeleeHitbox : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** Socket tren mesh lam tam hitbox (hand_r, hand_l, foot_r...) */
	UPROPERTY(EditAnywhere, Category = "Hitbox")
	FName SocketName = TEXT("hand_r");

	UPROPERTY(EditAnywhere, Category = "Hitbox")
	float Radius = 60.f;

	UPROPERTY(EditAnywhere, Category = "Hitbox")
	float Damage = 10.f;

	/** >0 thi hat nan nhan len (launcher L2 dung 650) */
	UPROPERTY(EditAnywhere, Category = "Hitbox")
	float LaunchZ = 0.f;

	/** GE phu ap them len nan nhan khi trung (enemy gan GE_PoisonDoT de doc player) */
	UPROPERTY(EditAnywhere, Category = "Hitbox")
	TSubclassOf<UGameplayEffect> ExtraEffectOnHit;

	/** Enemy bat len de khong danh trung dong loai — chi trung player */
	UPROPERTY(EditAnywhere, Category = "Hitbox")
	bool bOnlyHitPlayers = false;

	/** Only used when bOnlyHitPlayers is set. Stops Tick-driven enemy attacks from looping without a recovery beat. */
	UPROPERTY(EditAnywhere, Category = "Hitbox", meta = (ClampMin = "0.0"))
	float AttackCooldown = 1.25f;

	UPROPERTY(EditAnywhere, Category = "Hitbox")
	bool bDrawDebug = false;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

private:
	struct FHitboxState
	{
		TSet<TWeakObjectPtr<AActor>> HitActors;
		FVector PrevPos = FVector::ZeroVector;
	};

	/** State rieng cho tung mesh dang phat (key weak — khong giu song component) */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FHitboxState> ActiveStates;

	FVector GetHitboxLocation(const USkeletalMeshComponent* MeshComp) const;
};
