// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISense_Damage.h"
#include "StudentPerceptor.generated.h"

class AAIController;
class AActor;
class UBehaviorTreeComponent;
class UBlackboardComponent;

USTRUCT()
struct FCapturedStimulus
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;

	UPROPERTY()
	FString ActorName;

	UPROPERTY()
	FString ItemType;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	bool IsHouse() const
	{
		return ItemType.Equals(TEXT("House"), ESearchCase::IgnoreCase);
	}

	bool IsGarbage() const
	{
		return ItemType.Equals(TEXT("Garbage"), ESearchCase::IgnoreCase);
	}

	bool IsZombie() const
	{
		return ItemType.Equals(TEXT("Zombie"), ESearchCase::IgnoreCase);
	}

	bool IsValidTarget() const
	{
		return !IsGarbage() && !IsZombie() && (IsHouse() || ItemType.Equals(TEXT("Food"), ESearchCase::IgnoreCase)
			|| ItemType.Equals(TEXT("Medkit"), ESearchCase::IgnoreCase)
			|| ItemType.Equals(TEXT("Shotgun"), ESearchCase::IgnoreCase)
			|| ItemType.Equals(TEXT("Pistol"), ESearchCase::IgnoreCase));
	}
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WUYTSRUDYZOMBIERUNTIME_API UStudentPerceptor : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UStudentPerceptor();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION()
	virtual void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void StartStimulusCapture();
	void StartStimulusCapture(const FString& HouseActorName);
	void StopStimulusCapture();
	void ContinuouslyDiscoverHouses();
	bool GetNearestCapturedStimulus(const FVector& Origin, FCapturedStimulus& OutStimulus) const;
	bool GetNextRememberedHouse(const FString& CurrentHouseActorName, FCapturedStimulus& OutStimulus) const;
	bool GetNextQueuedPickupForActiveHouse(FCapturedStimulus& OutStimulus) const;
	FString GetActiveHouseActorName() const;
	bool GetNearestCapturedStimulusLocation(const FVector& Origin, FVector& OutLocation) const;
	void SeedCapturedStimuliFromCurrentPerception();
	void MarkStimulusVisited(const FString& VisitedActorName, const FVector& VisitedLocation);
	void RefreshActiveHousePickupQueue();

	// Found items management for quick-scan in houses
	void SetFoundItemsForCurrentHouse(const TArray<AActor*>& Items);
	bool GetNextFoundItem(AActor*& OutActor, FVector& OutLocation, FString& OutItemType);
	void RemoveCurrentFoundItem();
	int32 GetFoundItemCount() const;

protected:
	bool HasCapturedStimulus(const FString& ActorName, const FVector& StimulusLocation) const;
	bool IsStimulusVisited(const FString& ActorName, const FVector& StimulusLocation) const;
	void ForgetNonHouseStimuli();

	// Cached references to avoid repeated lookups
	UPROPERTY()
	class AAIController* CachedController = nullptr;

	UPROPERTY()
	class UBlackboardComponent* CachedBlackboard = nullptr;

	UPROPERTY()
	class UBehaviorTreeComponent* CachedBehaviorTree = nullptr;

	UPROPERTY()
	class UAIPerceptionComponent* CachedPerceptionComponent = nullptr;

	// Name of the blackboard key to set when we perceive an actor
	UPROPERTY(EditAnywhere, Category = "Perception")
	FName BlackboardTargetKey = FName(TEXT("TargetActor"));

	UPROPERTY(EditAnywhere, Category = "Perception")
	FName BlackboardTargetTypeKey = FName(TEXT("TargetItemType"));

	bool bIsCapturingStimuli = false;
	FString ActiveHouseActorName;
	TArray<FCapturedStimulus> CapturedStimuli;
	TArray<FCapturedStimulus> VisitedStimuli;
	TArray<FCapturedStimulus> KnownHouseStimuli;
	TMap<FString, TArray<FCapturedStimulus>> HousePickupQueues;

	// Found items from quick scan
	TArray<AActor*> FoundItems;
	int32 CurrentFoundItemIndex = 0;
};
