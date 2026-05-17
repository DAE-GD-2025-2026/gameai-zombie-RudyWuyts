#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BTTask_SpinAroundTarget.generated.h"

class AActor;

USTRUCT()
struct FBTSpinMemory
{
    GENERATED_BODY()

    float AccumulatedDeg;
    TWeakObjectPtr<AActor> TargetActor;
    FVector InitialOffset;

    FBTSpinMemory()
        : AccumulatedDeg(0.f)
        , TargetActor(nullptr)
        , InitialOffset(FVector::ZeroVector)
    {}
};

UCLASS(DisplayName = "Spin Around Target")
class WUYTSRUDYZOMBIERUNTIME_API UBTTask_SpinAroundTarget : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_SpinAroundTarget();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTSpinMemory); }

    // Blackboard key that points to the target Actor to spin around
    UPROPERTY(EditAnywhere, Category = "Spin")
    FBlackboardKeySelector TargetKey;

    // Degrees per second to rotate (around Z axis)
    UPROPERTY(EditAnywhere, Category = "Spin")
    float RotationSpeedDegPerSec;
};
