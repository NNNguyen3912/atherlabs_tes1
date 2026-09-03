#include "CombatWaveSpawner.h"

#include "CombatCharacterBase.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "TimerManager.h"

ACombatWaveSpawner::ACombatWaveSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ACombatWaveSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoStart)
	{
		StartWaves();
	}
}

void ACombatWaveSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnTimer);
		World->GetTimerManager().ClearTimer(NextWaveTimer);
	}
	ActiveEnemies.Empty();
	Super::EndPlay(EndPlayReason);
}

void ACombatWaveSpawner::StartWaves()
{
	if (bRunning || Waves.IsEmpty())
	{
		return;
	}

	bRunning = true;
	CurrentWaveIndex = 0;
	StartCurrentWave();
}

void ACombatWaveSpawner::StartCurrentWave()
{
	if (!bRunning || !Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	SpawnedThisWave = 0;
	bWaveCompletionQueued = false;
	OnWaveStarted.Broadcast(CurrentWaveIndex + 1);
	SpawnNextEnemy();
}

void ACombatWaveSpawner::SpawnNextEnemy()
{
	if (!bRunning || !Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	const FCombatWaveDefinition& Wave = Waves[CurrentWaveIndex];
	if (SpawnedThisWave >= Wave.EnemyCount)
	{
		EvaluateWaveCompletion();
		return;
	}

	if (!Wave.EnemyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s wave %d has no EnemyClass."), *GetName(), CurrentWaveIndex + 1);
		SpawnedThisWave = Wave.EnemyCount;
		EvaluateWaveCompletion();
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ACombatCharacterBase* Enemy = GetWorld()->SpawnActor<ACombatCharacterBase>(Wave.EnemyClass, GetSpawnTransform(SpawnedThisWave), SpawnParams);
	++SpawnedThisWave;

	if (Enemy)
	{
		ActiveEnemies.Add(Enemy);
		Enemy->OnCharacterDied.AddDynamic(this, &ACombatWaveSpawner::HandleEnemyDied);
		Enemy->OnDestroyed.AddDynamic(this, &ACombatWaveSpawner::HandleEnemyDestroyed);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s failed to spawn wave %d enemy %d."), *GetName(), CurrentWaveIndex + 1, SpawnedThisWave);
	}

	if (SpawnedThisWave < Wave.EnemyCount)
	{
		GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ACombatWaveSpawner::SpawnNextEnemy,
			FMath::Max(0.01f, Wave.SpawnInterval), false);
	}
	else
	{
		EvaluateWaveCompletion();
	}
}

void ACombatWaveSpawner::EvaluateWaveCompletion()
{
	if (!bRunning || bWaveCompletionQueued || !Waves.IsValidIndex(CurrentWaveIndex))
	{
		return;
	}

	const FCombatWaveDefinition& Wave = Waves[CurrentWaveIndex];
	if (SpawnedThisWave < Wave.EnemyCount || !ActiveEnemies.IsEmpty())
	{
		return;
	}

	bWaveCompletionQueued = true;
	OnWaveCompleted.Broadcast(CurrentWaveIndex + 1);
	GetWorld()->GetTimerManager().SetTimer(NextWaveTimer, this, &ACombatWaveSpawner::AdvanceToNextWave,
		FMath::Max(0.f, DelayBetweenWaves), false);
}

void ACombatWaveSpawner::AdvanceToNextWave()
{
	if (!bRunning)
	{
		return;
	}

	++CurrentWaveIndex;
	if (Waves.IsValidIndex(CurrentWaveIndex))
	{
		StartCurrentWave();
		return;
	}

	bRunning = false;
	OnAllWavesCompleted.Broadcast(FMath::Max(0, Waves.Num()));
}

FTransform ACombatWaveSpawner::GetSpawnTransform(int32 SpawnOrdinal) const
{
	FTransform Transform = GetActorTransform();
	if (!SpawnPoints.IsEmpty())
	{
		const int32 PointIndex = SpawnOrdinal % SpawnPoints.Num();
		if (const ATargetPoint* Point = SpawnPoints[PointIndex])
		{
			Transform = Point->GetActorTransform();
		}
	}
	Transform.AddToTranslation(FVector(0.f, 0.f, SpawnHeightOffset));
	return Transform;
}

void ACombatWaveSpawner::HandleEnemyDied(ACombatCharacterBase* DeadCharacter)
{
	RemoveActiveEnemy(DeadCharacter);
}

void ACombatWaveSpawner::HandleEnemyDestroyed(AActor* DestroyedActor)
{
	RemoveActiveEnemy(DestroyedActor);
}

void ACombatWaveSpawner::RemoveActiveEnemy(AActor* EnemyActor)
{
	if (!EnemyActor)
	{
		return;
	}

	const int32 RemovedCount = ActiveEnemies.Remove(EnemyActor);
	if (RemovedCount > 0)
	{
		EvaluateWaveCompletion();
	}
}
