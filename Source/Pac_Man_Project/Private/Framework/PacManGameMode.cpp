// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/PacManGameMode.h"

#include "PacManController.h"
#include "GridPawn.h"
#include "PickableActor.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/PacManGameInstance.h"
#include "PacManEnemyAIController.h"
#include "EnemyGridPawn.h"
#include "EnemyDataAsset.h"

APacManGameMode::APacManGameMode()
{
	/* Assign the class types used by this gamemode */
	PlayerControllerClass = APacManController::StaticClass();
	DefaultPawnClass = AGridPawn::StaticClass();
	
	static ConstructorHelpers::FObjectFinder<UEnemyDataAsset> PacManDataAssetRef(TEXT("/ Script / Pac_Man_Project.EnemyDataAsset'/Game/Data/PacManEnemies.PacManEnemies'"));
	if(PacManDataAssetRef.Succeeded())
		EnemiesData = PacManDataAssetRef.Object;

}

void APacManGameMode::FlipFlopScatterChase()
{


	if (GetWorldTimerManager().GetTimerRate(ScatterNChaseTimerHandle) == ScatterModeDuration) {
		GetWorldTimerManager().ClearTimer(ScatterNChaseTimerHandle);

		GetWorldTimerManager().SetTimer(ScatterNChaseTimerHandle, [&]() {FlipFlopScatterChase();}, ChaseModeDuration, false);

		OnChangeState.Broadcast(EEnemyState::Chase);

	}
	else {
		GetWorldTimerManager().ClearTimer(ScatterNChaseTimerHandle);

		GetWorldTimerManager().SetTimer(ScatterNChaseTimerHandle, [&]() {FlipFlopScatterChase();}, ScatterModeDuration, false);

		OnChangeState.Broadcast(EEnemyState::Scatter);
	}
}

void APacManGameMode::StartPlay()
{
	Super::StartPlay();

	TArray<TObjectPtr<AActor>> AllPickableScores;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APickableActor::StaticClass(), AllPickableScores);

	remainingScorePellets = AllPickableScores.Num();

	SpawnEnemies();

	GetWorldTimerManager().SetTimer(ScatterNChaseTimerHandle, [this]() {

		FlipFlopScatterChase();

	}, ScatterModeDuration, false);
}

void APacManGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorldTimerManager().ClearTimer(ScatterNChaseTimerHandle);
	GetWorldTimerManager().ClearTimer(FrightenedTimerHandle);

	
}

void APacManGameMode::BeginPlay()
{
	Super::BeginPlay();
}

void APacManGameMode::SpawnEnemies()
{
	FActorSpawnParameters ActorSpawnParams;
	ActorSpawnParams.bNoFail = true;
	ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	for (auto enemyType : TEnumRange<EEnemyType>())
	{
		//EEnemyType enemyType = EEnemyType::Blinky;
		TObjectPtr<APacManEnemyAIController> EnemyController = GetWorld()->SpawnActor<APacManEnemyAIController>();
		EnemyController->SetEnemyType(enemyType);

		TObjectPtr<AEnemyGridPawn> EnemyPawn = GetWorld()->SpawnActor<AEnemyGridPawn>(EnemyPawnClass, EnemiesData->GetEnemyInitialCell(enemyType), FRotator::ZeroRotator);
		EnemyPawn->InitMaterial(EnemiesData->GetEnemyMaterialColor(enemyType));

		EnemyController->Possess(EnemyPawn);

	}

}

void APacManGameMode::UpdateNCheckLevelCompleted()
{
	remainingScorePellets--;

	if (remainingScorePellets <= 0)
	{
		ReloadLevel();
	}
}

void APacManGameMode::ReloadLevel(bool bReduceLife)
{
	TObjectPtr<UPacManGameInstance> GI = GetWorld()->GetGameInstance<UPacManGameInstance>();

	if (GI)
	{
		if (bReduceLife)
		{
			//player collided with the enemy, reduce by 1 the lives and reload the same level
			//int remainingLives = 
			GI->SetLives(GI->GetLives() - 1);

			if (GI->GetLives() <= 0)
			{
				GI->OnGameOver.Broadcast();
			}
			else
			{
				UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), false);
			}
			
		}
		else {
			GI->IncrementLevel();
			UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), false);
		}

	}

}

void APacManGameMode::TriggerFrightened()
{
	if (GetWorldTimerManager().IsTimerActive(FrightenedTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(FrightenedTimerHandle);
	}

	GetWorldTimerManager().PauseTimer(ScatterNChaseTimerHandle);
	OnChangeState.Broadcast(EEnemyState::Frightened);

	GetWorldTimerManager().SetTimer(FrightenedTimerHandle, [this]() {

		OnChangeState.Broadcast(EEnemyState::Scatter);
		GetWorldTimerManager().UnPauseTimer(ScatterNChaseTimerHandle);

	}, FrightenedModeDuration, false);
}

void APacManGameMode::AddScoreFwd(int valueToAdd)
{
	TObjectPtr<UPacManGameInstance> GI = GetWorld()->GetGameInstance<UPacManGameInstance>();
	
	if (GI)
		GI->AddScore(valueToAdd);

	UpdateNCheckLevelCompleted();
}
