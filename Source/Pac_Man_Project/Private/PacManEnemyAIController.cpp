// Fill out your copyright notice in the Description page of Project Settings.


#include "PacManEnemyAIController.h"

#include "GridUtilities.h"
#include "Framework/PacManGameMode.h"
#include "EnemyGridPawn.h"
#include "Framework/PacManGameInstance.h"
#include "Kismet/GameplayStatics.h"

#pragma optimize("", off)


APacManEnemyAIController::APacManEnemyAIController()
{

}


void APacManEnemyAIController::BeginPlay()
{
	Super::BeginPlay();


}

void APacManEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (TObjectPtr<APacManGameMode> GM = Cast<APacManGameMode>(GetWorld()->GetAuthGameMode())) {
		EnemyInfo = GM->GetEnemiesData()->GetEnemyInfoPtr(EnemyType);
		GM->OnChangeState.AddDynamic(this, &APacManEnemyAIController::ChangeEnemyState);
	}

	ControlledGridPawn = Cast<AEnemyGridPawn>(InPawn);

	State = EEnemyState::Scatter;
	ChangeEnemyState(State);

	CurrentCell = VectorGridSnap(ControlledGridPawn->GetActorLocation());
	FVector NextDirection = DecideNextDirection();
	ControlledGridPawn->ForceDirection(NextDirection);

	NextCell = CurrentCell + NextDirection * GridConstants::GridSize;
}

void APacManEnemyAIController::ChangeEnemyState(EEnemyState NewState)
{
	//TObjectPtr<APacManGameMode> GM = Cast<APacManGameMode>(GetWorld()->GetAuthGameMode());
	//auto EnemyData = GM->GetEnemiesData();


	//If we are not in Idle, the usual logic applies so set the state and variables accordingly
	if (State != EEnemyState::Idle) {
		switch (NewState)
		{
		case EEnemyState::Scatter:
			TargetCell = EnemyInfo->ScatterCell;
			ControlledGridPawn->SetBlinkEffectMaterial(false);
			ControlledGridPawn->SetOpacityMaterial(1.f);
			ControlledGridPawn->ResetGridVelocity();

			break;

		case EEnemyState::Chase:
			ControlledGridPawn->SetBlinkEffectMaterial(false);
			ControlledGridPawn->SetOpacityMaterial(1.f);
			ControlledGridPawn->ResetGridVelocity();

			//Force Inverse Direction and an invalid next cell to force correct NextCell computation
			ControlledGridPawn->ForceDirection(-ControlledGridPawn->GetMovingDirection());
			NextCell = FVector::ZeroVector;
			//
			break;

		case EEnemyState::Frightened:
			ControlledGridPawn->SetBlinkEffectMaterial(true);
			ControlledGridPawn->SetOpacityMaterial(1.f);
			//Apply velocity malus only when entering the first time in the frightened mode
			if (State != EEnemyState::Frightened)
				ControlledGridPawn->SetToAlteredVelocity();

			//Force Inverse Direction and an invalid next cell to force correct NextCell computation
			ControlledGridPawn->ForceDirection(-ControlledGridPawn->GetMovingDirection());
			NextCell = FVector::ZeroVector;
			//
			break;

		default:
			//In any other case is Idle so return to the ghosthouse
			ControlledGridPawn->SetBlinkEffectMaterial(false);
			ControlledGridPawn->SetOpacityMaterial(0.f);
			TargetCell = EnemyInfo->InitialCell;
			ControlledGridPawn->ResetGridVelocity();
			break;

		}


		//Lastly update the state
		State = NewState;

	}


}


void APacManEnemyAIController::PawnOverlappedPlayerHandler()
{
	TObjectPtr<APacManGameMode> GM = Cast<APacManGameMode>(GetWorld()->GetAuthGameMode());

	if (GM) {

		if (State == EEnemyState::Frightened) {

			//Teleport and set to scatter
			//TODO: use Idle to move to initial cell rather than teleport
			/*
			GetWorldTimerManager().SetTimerForNextTick([GM,this]() {
				ControlledGridPawn->SetActorLocation(EnemyInfo->InitialCell);
				ChangeEnemyState(EEnemyState::Scatter);
			});
			*/
			ChangeEnemyState(EEnemyState::Idle);

			TObjectPtr<UPacManGameInstance> GI = GetWorld()->GetGameInstance<UPacManGameInstance>();
			if (GI)
			{
				GI->AddScore(ghostBaseValue, true);
			}
		}
		else {

			if(State != EEnemyState::Idle)
				GM->ReloadLevel(true);

		}
	}
}

FVector APacManEnemyAIController::DecideNextDirection(bool isChangingState)
{
	//Get Available directions: use linetrace to find free cells
	TArray<bool> AvailableDirections;
	FVector TraceStart = CurrentCell;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetPawn());

	for (auto& versor : GridConstants::GridVersorsArray) {
		FHitResult Hit;

		FVector TraceEnd = CurrentCell + versor * GridConstants::GridSize;
		GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldDynamic, QueryParams);
		DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Magenta);

		//We negate the logic of hit 
		AvailableDirections.Add(!Hit.bBlockingHit);
	}

	if (!isChangingState) 
	{
		//Exclude opposite direction
		int indexToExclude = -1;
		if (GridConstants::GridVersorsArray.Find(-ControlledGridPawn->GetMovingDirection(), indexToExclude))
			AvailableDirections[indexToExclude] = false;
	}
	/*
	//Find next direction among free cell by choosing the closest one
	float minDstSqrd = TNumericLimits<float>::Max();
	int minIdx = -1;

	for (int i = 0; i < AvailableDirections.Num(); i++)
	{
		if (!AvailableDirections[i])
			continue;

		FVector TmpNextCell = CurrentCell + GridConstants::GridVersorsArray[i] * GridConstants::GridSize;

		float dstSqrd = FMath::Pow(TmpNextCell.X - TargetCell.X, 2) + FMath::Pow(TmpNextCell.Y - TargetCell.Y, 2);

		if (dstSqrd < minDstSqrd) {
			minIdx = i;
			minDstSqrd = dstSqrd;
		}
	}


	return (minIdx>-1? GridConstants::GridVersorsArray[minIdx] : ControlledGridPawn->GetMovingDirection());
	*/
	return ApplyEnemyTypeDecision(AvailableDirections);
}

FVector APacManEnemyAIController::ApplyEnemyTypeDecision(const TArray<bool>& AvailableDirectionsArray)
{
	FVector DecidedDirection;
	switch (State)
	{
		case EEnemyState::Scatter:
			DecidedDirection = ClosestToTargetCellPolicy(AvailableDirectionsArray);

			break;

		case EEnemyState::Chase:

			UpdateChaseTargetCell();
			DecidedDirection = ClosestToTargetCellPolicy(AvailableDirectionsArray);

			break;

		case EEnemyState::Frightened:
			DecidedDirection = RandomChoicePolicy(AvailableDirectionsArray);

			break;

		default:
			//Idle or any other case
			DecidedDirection = ClosestToTargetCellPolicy(AvailableDirectionsArray);

			break;
	}

	return DecidedDirection;
}

void APacManEnemyAIController::UpdateChaseTargetCell()
{

	TObjectPtr<AGridPawn> PlayerPawn = Cast<AGridPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

	switch (EnemyType)
	{
	case EEnemyType::Pinky:	//Speedy - Pink
	
		TargetCell = VectorGridSnap(PlayerPawn->GetActorLocation() + PlayerPawn->GetMovingDirection() * 4 * GridConstants::GridSize);
		break;


	case EEnemyType::Inky:	//Bashful - Cyan
		{
			FVector IntermediateCell = VectorGridSnap(PlayerPawn->GetActorLocation() + PlayerPawn->GetMovingDirection() * 2 * GridConstants::GridSize);

			TArray<AActor*> outEnemyPCs;
			UGameplayStatics::GetAllActorsOfClass(GetWorld(), APacManEnemyAIController::StaticClass(), outEnemyPCs);

			AActor** pBlinkyPC = outEnemyPCs.FindByPredicate([](const AActor* Element) {
				return Cast<APacManEnemyAIController>(Element)->EnemyType == EEnemyType::Blinky;
			});

			APacManEnemyAIController* BlinkyPC = Cast<APacManEnemyAIController>(*pBlinkyPC);
			FVector BlinkyPosition = BlinkyPC->ControlledGridPawn->GetActorLocation();

			FVector DistToRot = BlinkyPosition - IntermediateCell;
			DistToRot.RotateAngleAxis(180.0, FVector::UpVector);

			TargetCell = VectorGridSnap(IntermediateCell + DistToRot);
		}
		break;


	case EEnemyType::Clyde:	//Pokey - Orange:
		{
			double ToPlayerDistance = (PlayerPawn->GetActorLocation() - ControlledGridPawn->GetActorLocation()).Size();
			if (ToPlayerDistance < 8 * GridConstants::GridSize)
				TargetCell = EnemyInfo->ScatterCell;
			else
				TargetCell = VectorGridSnap(PlayerPawn->GetActorLocation());
		}
		break;


	default:
			//Blinky: Shadow - Red or any other case
		TargetCell = VectorGridSnap(PlayerPawn->GetActorLocation());

		break;
	}

}


FVector APacManEnemyAIController::RandomChoicePolicy(const TArray<bool>& AvailableDirectionsArray)
{
	auto Directions = GridConstants::GridVersorsArray;
	for (size_t i = 0; i < AvailableDirectionsArray.Num(); i++)
	{
		if (!AvailableDirectionsArray[i]) {
			Directions.Remove(GridConstants::GridVersorsArray[i]);
		}
	}

	int randIdx = FMath::RandRange(0, Directions.Num()-1);

	return Directions[randIdx];
}


FVector APacManEnemyAIController::ClosestToTargetCellPolicy(const TArray<bool>& AvailableDirectionsArray)
{

	//Find next direction among free cell by choosing the closest one
	float minDstSqrd = TNumericLimits<float>::Max();
	int minIdx = -1;

	for (int i = 0; i < AvailableDirectionsArray.Num(); i++)
	{
		if (!AvailableDirectionsArray[i])
			continue;

		FVector TmpNextCell = CurrentCell + GridConstants::GridVersorsArray[i] * GridConstants::GridSize;

		float dstSqrd = FMath::Pow(TmpNextCell.X - TargetCell.X, 2) + FMath::Pow(TmpNextCell.Y - TargetCell.Y, 2);

		if (dstSqrd < minDstSqrd) {
			minIdx = i;
			minDstSqrd = dstSqrd;
		}
	}


	return (minIdx > -1 ? GridConstants::GridVersorsArray[minIdx] : ControlledGridPawn->GetMovingDirection());

}


void APacManEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CurrentCell = VectorGridSnap(ControlledGridPawn->GetActorLocation());
	FVector CurrentLocation = ControlledGridPawn->GetActorLocation();

	bool hasReachedNextCell = (CurrentLocation - NextCell).Size() < 12.5f;
	bool hasSkippedNextCell = (CurrentCell - NextCell).Size() > GridConstants::GridSize;
	bool isStuck = ControlledGridPawn->GetVelocity().SizeSquared2D() <= 0.f;

	/*
	if (hasReachedNextCell)
		DrawDebugCircle(GetWorld(), ControlledGridPawn->GetActorLocation(), 25, 25,
			FColor::Green, false, 2, 0, 0, FVector::RightVector, FVector::ForwardVector);*/
	if (hasSkippedNextCell)
		DrawDebugCircle(GetWorld(), NextCell, 25, 25,
			FColor::Purple, false, 2, 0, 0, FVector::RightVector, FVector::ForwardVector);
	/*if (isStuck)
		GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Orange, FString::Printf(TEXT("SONO STUCK!")));
	*/

	if (hasReachedNextCell ||		//The new cell is recomputed if we reach the next cell
		hasSkippedNextCell)// ||	//Error recovery: enemy is moving far from next cell
		//isStuck) 					//Error recovery: enemy is stuck
	{
		FVector NextDirection = DecideNextDirection();
		ControlledGridPawn->ForceDirection(NextDirection);

		NextCell = CurrentCell + NextDirection * GridConstants::GridSize;

	}

	if (State == EEnemyState::Idle && ((CurrentLocation - TargetCell).Size() < 12.5f || NextCell == TargetCell))
	{
		State = EEnemyState::Scatter;
		ChangeEnemyState(State);
	}

	DrawDebugCircle(GetWorld(), TargetCell, 25, 25, EnemyInfo->EnemyColor,false, 1, 0, 0, FVector::RightVector, FVector::ForwardVector);

	DrawDebugLine(GetWorld(), ControlledGridPawn->GetActorLocation(), ControlledGridPawn->GetActorLocation()+ControlledGridPawn->GetMovingDirection()*100, FColor::Green);
}

#pragma optimize("", on)
