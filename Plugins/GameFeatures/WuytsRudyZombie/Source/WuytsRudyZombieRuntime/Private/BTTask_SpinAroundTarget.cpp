#include "BTTask_SpinAroundTarget.h"
#include "../StudentPerceptor.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_String.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogBTSpin, Log, All);

UBTTask_SpinAroundTarget::UBTTask_SpinAroundTarget()
{
    NodeName = TEXT("Spin Around Target");
    bNotifyTick = true;
    RotationSpeedDegPerSec = 180.f;

    // Restrict the blackboard key to Actor types in editor
    TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SpinAroundTarget, TargetKey), AActor::StaticClass());
    PositionTargetKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SpinAroundTarget, PositionTargetKey));
    TargetItemTypeKey.AddStringFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_SpinAroundTarget, TargetItemTypeKey));
}

EBTNodeResult::Type UBTTask_SpinAroundTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    FBTSpinMemory* MyMemory = reinterpret_cast<FBTSpinMemory*>(NodeMemory);
    MyMemory->AccumulatedDeg = 0.f;

    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    if (!BlackboardComp)
    {
        UE_LOG(LogBTSpin, Warning, TEXT("SpinAround: Blackboard component missing"));
        return EBTNodeResult::Failed;
    }

    UObject* Obj = BlackboardComp->GetValueAsObject(TargetKey.SelectedKeyName.IsNone()
        ? FName(TEXT("TargetActor"))
        : TargetKey.SelectedKeyName);

    AActor* Target = Cast<AActor>(Obj);
    if (!Target)
    {
        UE_LOG(LogBTSpin, Warning, TEXT("SpinAround: Target is null"));
        return EBTNodeResult::Failed;
    }

    MyMemory->TargetActor = Target;
    MyMemory->TargetItemType = BlackboardComp->GetValueAsString(TargetItemTypeKey.SelectedKeyName.IsNone()
        ? FName(TEXT("TargetItemType"))
        : TargetItemTypeKey.SelectedKeyName);

    if (APawn* Pawn = OwnerComp.GetAIOwner() ? OwnerComp.GetAIOwner()->GetPawn() : nullptr)
    {
        if (UStudentPerceptor* Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>())
        {
            if (MyMemory->TargetItemType.Equals(TEXT("House"), ESearchCase::IgnoreCase))
            {
                Perceptor->StartStimulusCapture(Target->GetName());
            }
            else
            {
                Perceptor->StartStimulusCapture();
            }
        }
    }

    if (AAIController* AICon = OwnerComp.GetAIOwner())
    {
        if (APawn* Pawn = AICon->GetPawn())
        {
            AICon->StopMovement();
        }
    }

    UE_LOG(LogBTSpin, Log, TEXT("SpinAround: Starting spin around %s"), *Target->GetName());

    return EBTNodeResult::InProgress;
}

void UBTTask_SpinAroundTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    FBTSpinMemory* MyMemory = reinterpret_cast<FBTSpinMemory*>(NodeMemory);
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    APawn* Pawn = AICon->GetPawn();
    if (!Pawn)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    AActor* Target = MyMemory->TargetActor.Get();
    if (!Target)
    {
        if (UStudentPerceptor* Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>())
        {
            Perceptor->StopStimulusCapture();
        }

        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // Continuously refresh items during the spin to capture anything newly perceived
    if (UStudentPerceptor* Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>())
    {
        if (MyMemory->TargetItemType.Equals(TEXT("House"), ESearchCase::IgnoreCase))
        {
            Perceptor->RefreshActiveHousePickupQueue();
        }
    }

    const float Step = RotationSpeedDegPerSec * DeltaSeconds;
    MyMemory->AccumulatedDeg += Step;

    Pawn->AddActorWorldRotation(FRotator(0.f, Step, 0.f));
    if (UPawnMovementComponent* MoveComp = Pawn->GetMovementComponent())
    {
        MoveComp->StopMovementImmediately();
        MoveComp->Velocity = FVector::ZeroVector;
    }

    if (MyMemory->AccumulatedDeg < 360.f)
    {
        return;
    }

    UE_LOG(LogBTSpin, Log, TEXT("SpinAround: Completed 360 degrees"));

    if (UStudentPerceptor* Perceptor = Pawn->FindComponentByClass<UStudentPerceptor>())
    {
        Perceptor->StopStimulusCapture();

        // Final refresh to catch any last items
        if (MyMemory->TargetItemType.Equals(TEXT("House"), ESearchCase::IgnoreCase))
        {
            Perceptor->RefreshActiveHousePickupQueue();
        }

        // First, try to get a queued pickup for this house
        FCapturedStimulus NextTarget;
        if (Perceptor->GetNextQueuedPickupForActiveHouse(NextTarget))
        {
            const FName TargetActorKeyName = TargetKey.SelectedKeyName.IsNone()
                ? FName(TEXT("TargetActor"))
                : TargetKey.SelectedKeyName;
            const FName PositionKeyName = PositionTargetKey.SelectedKeyName.IsNone()
                ? FName(TEXT("PositionTarget"))
                : PositionTargetKey.SelectedKeyName;
            const FName TargetTypeKeyName = TargetItemTypeKey.SelectedKeyName.IsNone()
                ? FName(TEXT("TargetItemType"))
                : TargetItemTypeKey.SelectedKeyName;

            if (BlackboardComp)
            {
                BlackboardComp->SetValueAsObject(TargetActorKeyName, NextTarget.Actor.Get());
                BlackboardComp->SetValueAsVector(PositionKeyName, NextTarget.Location);
                BlackboardComp->SetValueAsString(TargetTypeKeyName, NextTarget.ItemType);
                UE_LOG(LogBTSpin, Log, TEXT("SpinAround: Queued pickup target %s type %s location %s"), *NextTarget.ActorName, *NextTarget.ItemType, *NextTarget.Location.ToString());
                FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
                return;
            }

            UE_LOG(LogBTSpin, Warning, TEXT("SpinAround: Blackboard missing when trying to write queued pickup target"));
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }

        UE_LOG(LogBTSpin, Warning, TEXT("SpinAround: No pickup queue found for active house %s"), *Perceptor->GetActiveHouseActorName());

        // If no pickups queued for this house, try to find next house
        FCapturedStimulus NextHouse;
        if (Perceptor->GetNextRememberedHouse(Perceptor->GetActiveHouseActorName(), NextHouse))
        {
            const FName TargetActorKeyName = TargetKey.SelectedKeyName.IsNone()
                ? FName(TEXT("TargetActor"))
                : TargetKey.SelectedKeyName;
            const FName PositionKeyName = PositionTargetKey.SelectedKeyName.IsNone()
                ? FName(TEXT("PositionTarget"))
                : PositionTargetKey.SelectedKeyName;
            const FName TargetTypeKeyName = TargetItemTypeKey.SelectedKeyName.IsNone()
                ? FName(TEXT("TargetItemType"))
                : TargetItemTypeKey.SelectedKeyName;

            if (BlackboardComp)
            {
                BlackboardComp->SetValueAsObject(TargetActorKeyName, NextHouse.Actor.Get());
                BlackboardComp->SetValueAsVector(PositionKeyName, NextHouse.Location);
                BlackboardComp->SetValueAsString(TargetTypeKeyName, NextHouse.ItemType);
                UE_LOG(LogBTSpin, Log, TEXT("SpinAround: No pickup queue, falling back to remembered house %s at %s"), *NextHouse.ActorName, *NextHouse.Location.ToString());
                FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
                return;
            }

            UE_LOG(LogBTSpin, Warning, TEXT("SpinAround: Blackboard missing when trying to write fallback house target"));
            FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
            return;
        }

        // No pickups and no next house - this is expected if this is the last house
        UE_LOG(LogBTSpin, Log, TEXT("SpinAround: No queued pickup and no remembered house was available. Ending with success."));
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }

    UE_LOG(LogBTSpin, Warning, TEXT("SpinAround: StudentPerceptor not found when completing spin"));
    FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
}
