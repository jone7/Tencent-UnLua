// Tencent is pleased to support the open source community by making UnLua available.
// 
// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#include "LuaOverridesClass.h"
#include "LuaFunction.h"
#include "Misc/EngineVersionComparison.h"

ULuaOverridesClass* ULuaOverridesClass::Create(UClass* Class)
{
    auto ClassNameString = FString::Printf(TEXT("LUA_OVERRIDES_%s"), *Class->GetName());
    auto ClassName = MakeUniqueObjectName(GetTransientPackage(), Class, FName(*ClassNameString));
    auto Ret = NewObject<ULuaOverridesClass>(GetTransientPackage(), ClassName, RF_Public | RF_Transient);
    Ret->ClassFlags |= CLASS_NewerVersionExists; // bypass FBlueprintActionDatabase::RefreshClassActions
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5,6,0)
    Ret->SetDefaultObject(StaticClass()->GetDefaultObject());
#else
    Ret->ClassDefaultObject = StaticClass()->GetDefaultObject();
#endif
    Ret->SetSuperStruct(StaticClass());
    Ret->Bind();
    Ret->Owner = Class;
    Ret->AddToOwner();
    return Ret;
}

void ULuaOverridesClass::Restore()
{
    SetActive(false);
    RemoveFromOwner();
}

void ULuaOverridesClass::SetActive(const bool bActive)
{
    const auto Class = Owner.Get();
    if (!Class)
        return;

    for (TFieldIterator<ULuaFunction> It(this, EFieldIteratorFlags::ExcludeSuper); It; ++It)
    {
        const auto LuaFunction = *It;
        LuaFunction->SetActive(bActive);
    }

    Class->ClearFunctionMapsCaches();
    if (bActive)
        AddToOwner();
    else
        RemoveFromOwner();
}

void ULuaOverridesClass::BeginDestroy()
{
    Restore();
    UClass::BeginDestroy();
}

void ULuaOverridesClass::AddToOwner()
{
    const auto Class = Owner.Get();
    if (!Class)
        return;

    // Check if already added by traversing the linked list
    UField* Current = Class->Children;
    while (Current)
    {
        if (Current == this)
            goto AlreadyAdded;
        Current = Current->Next;
    }

    // Not found, prepend to the Children linked list
    this->Next = Class->Children;
    Class->Children = this;

AlreadyAdded:
    if (Class->IsRooted() || GUObjectArray.IsDisregardForGC(Class))
        AddToRoot();
}

void ULuaOverridesClass::RemoveFromOwner()
{
    const auto Class = Owner.Get();
    if (!Class)
        return;

    // If this is the first child, update Children directly
    if (Class->Children == this)
    {
        Class->Children = nullptr;
    }
    else
    {
        // Traverse the linked list to find and unlink this node
        UField* Current = Class->Children;
        while (Current)
        {
            if (Current->Next == this)
            {
                Current->Next = nullptr;
                break;
            }
            Current = Current->Next;
        }
    }

    if (!Class->IsRooted() && !GUObjectArray.IsDisregardForGC(Class))
        RemoveFromRoot();
}
