// Fill out your copyright notice in the Description page of Project Settings.


#include "StudentPerceptor.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Perception/AISense_Sight.h"


UStudentPerceptor::UStudentPerceptor()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStudentPerceptor::BeginPlay()
{
	Super::BeginPlay();
	
	if (auto PerceptionComp = GetOwner()->GetComponentByClass<UAIPerceptionComponent>())
	{
		CachedPerceptionComponent = PerceptionComp;
		PerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &UStudentPerceptor::OnPerceptionUpdated);
	}

	APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (PawnOwner)
	{
		CachedController = Cast<AAIController>(PawnOwner->GetController());
		if (CachedController)
		{
			CachedBlackboard = CachedController->GetBlackboardComponent();
			CachedBehaviorTree = Cast<UBehaviorTreeComponent>(CachedController->BrainComponent);
		}
	}
}

void UStudentPerceptor::StartStimulusCapture()
{
	bIsCapturingStimuli = true;
	SeedCapturedStimuliFromCurrentPerception();
}

void UStudentPerceptor::StartStimulusCapture(const FString& HouseActorName)
{
	bIsCapturingStimuli = true;
	if (!HouseActorName.IsEmpty())
	{
		ActiveHouseActorName = HouseActorName;
	}

	SeedCapturedStimuliFromCurrentPerception();
}

void UStudentPerceptor::StopStimulusCapture()
{
	bIsCapturingStimuli = false;
	ForgetNonHouseStimuli();
}

namespace
{
	FString ResolveStimulusType(const FString& ActorName)
	{
		if (ActorName.Contains(TEXT("Garbage"), ESearchCase::IgnoreCase)) return TEXT("Garbage");
		if (ActorName.Contains(TEXT("Zombie"), ESearchCase::IgnoreCase)) return TEXT("Zombie");
		if (ActorName.Contains(TEXT("Food"), ESearchCase::IgnoreCase)) return TEXT("Food");
		if (ActorName.Contains(TEXT("Medkit"), ESearchCase::IgnoreCase)) return TEXT("Medkit");
		if (ActorName.Contains(TEXT("Shotgun"), ESearchCase::IgnoreCase)) return TEXT("Shotgun");
		if (ActorName.Contains(TEXT("Pistol"), ESearchCase::IgnoreCase)) return TEXT("Pistol");
		if (ActorName.Contains(TEXT("House"), ESearchCase::IgnoreCase)) return TEXT("House");
		return TEXT("Unknown");
	}

	bool IsPickupItemType(const FString& ItemType)
	{
		return ItemType.Equals(TEXT("Food"), ESearchCase::IgnoreCase)
			|| ItemType.Equals(TEXT("Medkit"), ESearchCase::IgnoreCase)
			|| ItemType.Equals(TEXT("Shotgun"), ESearchCase::IgnoreCase)
			|| ItemType.Equals(TEXT("Pistol"), ESearchCase::IgnoreCase);
	}

	bool HasMatchingStimulus(const TArray<FCapturedStimulus>& Stimuli, const FString& ActorName, const FVector& Location)
	{
		for (const FCapturedStimulus& Stimulus : Stimuli)
		{
			if (Stimulus.ActorName.Equals(ActorName, ESearchCase::IgnoreCase)
				&& FVector::DistSquared(Stimulus.Location, Location) <= FMath::Square(100.f))
			{
				return true;
			}
		}

		return false;
	}

	void RememberHouseStimulus(TArray<FCapturedStimulus>& KnownHouseStimuli, const FCapturedStimulus& Stimulus)
	{
		if (!Stimulus.IsHouse() || HasMatchingStimulus(KnownHouseStimuli, Stimulus.ActorName, Stimulus.Location))
		{
			return;
		}

		KnownHouseStimuli.Add(Stimulus);
	}

	void RememberPickupStimulus(TMap<FString, TArray<FCapturedStimulus>>& HousePickupQueues, const FString& HouseActorName, const FCapturedStimulus& Stimulus)
	{
		if (HouseActorName.IsEmpty() || !IsPickupItemType(Stimulus.ItemType))
		{
			return;
		}

		TArray<FCapturedStimulus>& Queue = HousePickupQueues.FindOrAdd(HouseActorName);
		if (HasMatchingStimulus(Queue, Stimulus.ActorName, Stimulus.Location))
		{
			return;
		}

		Queue.Add(Stimulus);
	}

	void RemoveStimulusFromQueues(TMap<FString, TArray<FCapturedStimulus>>& HousePickupQueues, const FString& ActorName, const FVector& Location)
	{
		for (TPair<FString, TArray<FCapturedStimulus>>& QueuePair : HousePickupQueues)
		{
			QueuePair.Value.RemoveAll([&ActorName, &Location](const FCapturedStimulus& Stimulus)
			{
				return Stimulus.ActorName.Equals(ActorName, ESearchCase::IgnoreCase)
					&& FVector::DistSquared(Stimulus.Location, Location) <= FMath::Square(100.f);
			});
		}
	}
}

FString UStudentPerceptor::GetActiveHouseActorName() const
{
	return ActiveHouseActorName;
}

bool UStudentPerceptor::GetNearestCapturedStimulus(const FVector& Origin, FCapturedStimulus& OutStimulus) const
{
	if (CapturedStimuli.IsEmpty())
	{
		const_cast<UStudentPerceptor*>(this)->SeedCapturedStimuliFromCurrentPerception();
	}

	bool bFoundStimulus = false;
	float BestDistanceSq = TNumericLimits<float>::Max();

	for (const FCapturedStimulus& Stimulus : CapturedStimuli)
	{
		if (!Stimulus.IsValidTarget() || IsStimulusVisited(Stimulus.ActorName, Stimulus.Location))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Origin, Stimulus.Location);
		if (DistanceSq < BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			OutStimulus = Stimulus;
			bFoundStimulus = true;
		}
	}

	return bFoundStimulus;
}

bool UStudentPerceptor::GetNextQueuedPickupForActiveHouse(FCapturedStimulus& OutStimulus) const
{
	if (ActiveHouseActorName.IsEmpty())
	{
		return false;
	}

	const TArray<FCapturedStimulus>* Queue = HousePickupQueues.Find(ActiveHouseActorName);
	if (!Queue)
	{
		return false;
	}

	for (const FCapturedStimulus& Candidate : *Queue)
	{
		if (!IsPickupItemType(Candidate.ItemType)
			|| IsStimulusVisited(Candidate.ActorName, Candidate.Location))
		{
			continue;
		}

		OutStimulus = Candidate;
		return true;
	}

	return false;
}

bool UStudentPerceptor::GetNextRememberedHouse(const FString& CurrentHouseActorName, FCapturedStimulus& OutStimulus) const
{
	if (KnownHouseStimuli.IsEmpty())
	{
		return false;
	}

	int32 StartIndex = 0;
	if (!CurrentHouseActorName.IsEmpty())
	{
		for (int32 Index = 0; Index < KnownHouseStimuli.Num(); ++Index)
		{
			if (KnownHouseStimuli[Index].ActorName.Equals(CurrentHouseActorName, ESearchCase::IgnoreCase))
			{
				StartIndex = Index + 1;
				break;
			}
		}
	}

	for (int32 Offset = 0; Offset < KnownHouseStimuli.Num(); ++Offset)
	{
		const int32 Index = (StartIndex + Offset) % KnownHouseStimuli.Num();
		const FCapturedStimulus& Candidate = KnownHouseStimuli[Index];
		if (!Candidate.IsHouse()
			|| Candidate.ActorName.Equals(CurrentHouseActorName, ESearchCase::IgnoreCase)
			|| IsStimulusVisited(Candidate.ActorName, Candidate.Location))
		{
			continue;
		}

		OutStimulus = Candidate;
		return true;
	}

	return false;
}

bool UStudentPerceptor::GetNearestCapturedStimulusLocation(const FVector& Origin, FVector& OutLocation) const
{
	FCapturedStimulus NearestStimulus;
	if (GetNearestCapturedStimulus(Origin, NearestStimulus))
	{
		OutLocation = NearestStimulus.Location;
		return true;
	}

	return false;
}

void UStudentPerceptor::SeedCapturedStimuliFromCurrentPerception()
{
	if (!CachedPerceptionComponent)
	{
		return;
	}

	TArray<AActor*> PerceivedActors;
	CachedPerceptionComponent->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

	for (AActor* PerceivedActor : PerceivedActors)
	{
		if (PerceivedActor)
		{
			const FString ActorName = PerceivedActor->GetName();
			const FString ItemType = ResolveStimulusType(ActorName);
			if (ItemType.Equals(TEXT("Garbage"), ESearchCase::IgnoreCase) || ItemType.Equals(TEXT("Unknown"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			const FCapturedStimulus Stimulus
			{
 				PerceivedActor,
				ActorName,
				ItemType,
				PerceivedActor->GetActorLocation()
			};

			RememberHouseStimulus(KnownHouseStimuli, Stimulus);
			RememberPickupStimulus(HousePickupQueues, ActiveHouseActorName, Stimulus);

			if (Stimulus.IsValidTarget() && !IsStimulusVisited(Stimulus.ActorName, Stimulus.Location) && !HasCapturedStimulus(Stimulus.ActorName, Stimulus.Location))
			{
				CapturedStimuli.Add(Stimulus);
			}
		}
	}
}

void UStudentPerceptor::MarkStimulusVisited(const FString& VisitedActorName, const FVector& VisitedLocation)
{
	const FCapturedStimulus VisitedStimulus
	{
		nullptr,
		VisitedActorName,
		ResolveStimulusType(VisitedActorName),
		VisitedLocation
	};

	if (!IsStimulusVisited(VisitedStimulus.ActorName, VisitedStimulus.Location))
	{
		VisitedStimuli.Add(VisitedStimulus);
	}

	RemoveStimulusFromQueues(HousePickupQueues, VisitedStimulus.ActorName, VisitedStimulus.Location);

	CapturedStimuli.RemoveAll([&VisitedStimulus](const FCapturedStimulus& Stimulus)
	{
		return Stimulus.ActorName.Equals(VisitedStimulus.ActorName, ESearchCase::IgnoreCase)
			&& FVector::DistSquared(Stimulus.Location, VisitedStimulus.Location) <= FMath::Square(100.f);
	});
}

bool UStudentPerceptor::HasCapturedStimulus(const FString& ActorName, const FVector& StimulusLocation) const
{
	for (const FCapturedStimulus& Stimulus : CapturedStimuli)
	{
		if (Stimulus.ActorName.Equals(ActorName, ESearchCase::IgnoreCase)
			&& FVector::DistSquared(Stimulus.Location, StimulusLocation) <= FMath::Square(100.f))
		{
			return true;
		}
	}

	return false;
}

bool UStudentPerceptor::IsStimulusVisited(const FString& ActorName, const FVector& StimulusLocation) const
{
	for (const FCapturedStimulus& VisitedStimulus : VisitedStimuli)
	{
		if (VisitedStimulus.ActorName.Equals(ActorName, ESearchCase::IgnoreCase)
			&& FVector::DistSquared(VisitedStimulus.Location, StimulusLocation) <= FMath::Square(100.f))
		{
			return true;
		}
	}

	return false;
}

void UStudentPerceptor::ForgetNonHouseStimuli()
{
	CapturedStimuli.RemoveAll([](const FCapturedStimulus& Stimulus)
	{
		return !Stimulus.IsHouse();
	});

	VisitedStimuli.RemoveAll([](const FCapturedStimulus& Stimulus)
	{
		return !Stimulus.IsHouse();
	});
}

void UStudentPerceptor::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor)
	{
		return;
	}

	FString StimulusInfo = FString::Printf(
		TEXT("Stimulus - Tag: %s, Strength: %.2f, Age: %.2f, StimulusLocation: (%.1f,%.1f,%.1f), ReceiverLocation: (%.1f,%.1f,%.1f), WasSensed: %s, Expired: %s"),
		*Stimulus.Tag.ToString(),
		Stimulus.Strength,
		Stimulus.GetAge(),
		Stimulus.StimulusLocation.X, Stimulus.StimulusLocation.Y, Stimulus.StimulusLocation.Z,
		Stimulus.ReceiverLocation.X, Stimulus.ReceiverLocation.Y, Stimulus.ReceiverLocation.Z,
		Stimulus.WasSuccessfullySensed() ? TEXT("true") : TEXT("false"),
		Stimulus.IsExpired() ? TEXT("true") : TEXT("false")
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, StimulusInfo);
	}
	UE_LOG(LogTemp, Log, TEXT("%s"), *StimulusInfo);

	if (bIsCapturingStimuli)
	{
		if (Stimulus.WasSuccessfullySensed())
		{
			const FCapturedStimulus CapturedStimulus
			{
				Actor,
				Actor->GetName(),
				ResolveStimulusType(Actor->GetName()),
				Stimulus.StimulusLocation
			};

			RememberHouseStimulus(KnownHouseStimuli, CapturedStimulus);
			RememberPickupStimulus(HousePickupQueues, ActiveHouseActorName, CapturedStimulus);

			if (CapturedStimulus.IsValidTarget()
				&& !IsStimulusVisited(CapturedStimulus.ActorName, CapturedStimulus.Location)
				&& !HasCapturedStimulus(CapturedStimulus.ActorName, CapturedStimulus.Location))
			{
				CapturedStimuli.Add(CapturedStimulus);
			}
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Green, TEXT("Saw Something!"));
			}
		}
	}

	if (CachedBlackboard)
	{
		CachedBlackboard->SetValueAsObject(BlackboardTargetKey, Actor);
	}
	else
	{
		APawn* PawnOwner = Cast<APawn>(GetOwner());
		if (PawnOwner)
		{
			if (AAIController* AICon = Cast<AAIController>(PawnOwner->GetController()))
			{
				if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
				{
					BB->SetValueAsObject(BlackboardTargetKey, Actor);
					CachedBlackboard = BB;
					CachedController = AICon;
					CachedBehaviorTree = Cast<UBehaviorTreeComponent>(AICon->BrainComponent);
				}
			}
		}
	}

	if (CachedBehaviorTree)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Blue, TEXT("BehaviorTreeComponent found"));
	}
}
