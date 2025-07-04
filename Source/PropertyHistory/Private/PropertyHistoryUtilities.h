// Copyright Voxel Plugin SAS. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define PROPERTY_HISTORY_ENGINE_VERSION (ENGINE_MAJOR_VERSION * 100 + ENGINE_MINOR_VERSION)

struct FLambdaCaller
{
	template<typename T>
	FORCEINLINE auto operator+(T&& Lambda) -> decltype(auto)
	{
		return Lambda();
	}
};

#define INLINE_LAMBDA FLambdaCaller() + [&]()

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Usage: DEFINE_PRIVATE_ACCESS(FMyClass, MyProperty) in global scope, then PrivateAccess::MyProperty(MyObject) from anywhere
#define DEFINE_PRIVATE_ACCESS(Class, Property) \
	namespace PrivateAccess \
	{ \
		template<typename> \
		struct TClass_ ## Property; \
		\
		template<> \
		struct TClass_ ## Property<Class> \
		{ \
			template<auto PropertyPtr> \
			struct TProperty_ ## Property \
			{ \
				friend auto& Property(Class& Object) \
				{ \
					return Object.*PropertyPtr; \
				} \
				friend auto& Property(const Class& Object) \
				{ \
					return Object.*PropertyPtr; \
				} \
			}; \
		}; \
		template struct TClass_ ## Property<Class>::TProperty_ ## Property<&Class::Property>; \
		\
		auto& Property(Class& Object); \
		auto& Property(const Class& Object); \
	}

#define DEFINE_PRIVATE_ACCESS_FUNCTION(Class, Function) \
	namespace PrivateAccess \
	{ \
		template<typename> \
		struct TClass_ ## Function; \
		\
		template<> \
		struct TClass_ ## Function<Class> \
		{ \
			template<auto FunctionPtr> \
			struct TFunction_ ## Function \
			{ \
				friend auto Function(Class& Object) \
				{ \
					return [&Object]<typename... ArgTypes>(ArgTypes&&... Args) \
					{ \
						return (Object.*FunctionPtr)(Forward<ArgTypes>(Args)...); \
					}; \
				} \
			}; \
		}; \
		template struct TClass_ ## Function<Class>::TFunction_ ## Function<&Class::Function>; \
		\
		auto Function(Class& Object); \
		auto Function(const Class& Object) \
		{ \
			return Function(const_cast<Class&>(Object)); \
		} \
	}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename... ArgTypes>
struct TVoxelTypes
{
	static constexpr int32 Num = sizeof...(ArgTypes);

	template<int32 Index>
	using Get = typename TTupleElement<Index, TTuple<ArgTypes...>>::Type;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename>
struct TVoxelFunctionInfo;

template<typename InReturnType, typename... InArgTypes>
struct TVoxelFunctionInfo<InReturnType(InArgTypes...)>
{
	using ReturnType = InReturnType;
	using ArgTypes = TVoxelTypes<InArgTypes...>;
	using Signature = InReturnType(InArgTypes...);
};

template<typename ReturnType, typename InClass, typename... ArgTypes>
struct TVoxelFunctionInfo<ReturnType(InClass::*)(ArgTypes...) const> : TVoxelFunctionInfo<ReturnType(ArgTypes...)>
{
	using Class = InClass;
};

template<typename ReturnType, typename InClass, typename... ArgTypes>
struct TVoxelFunctionInfo<ReturnType(InClass::*)(ArgTypes...)> : TVoxelFunctionInfo<ReturnType(ArgTypes...)>
{
	using Class = InClass;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename LambdaType>
struct TVoxelLambdaInfo
{
	static_assert(sizeof(LambdaType) == -1, "LambdaType is not a lambda. Note that generic lambdas (eg [](auto)) are not supported.");
};

template<typename LambdaType>
requires (sizeof(decltype(&LambdaType::operator())) != 0)
struct TVoxelLambdaInfo<LambdaType> : TVoxelFunctionInfo<decltype(&LambdaType::operator())>
{
};

template<typename LambdaType>
struct TVoxelLambdaInfo<LambdaType&> : TVoxelLambdaInfo<LambdaType>
{
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename Signature>
using FunctionReturnType_T = typename TVoxelFunctionInfo<Signature>::ReturnType;

template<typename Signature>
using FunctionArgTypes_T = typename TVoxelFunctionInfo<Signature>::ArgTypes;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename LambdaType>
using LambdaReturnType_T = typename TVoxelLambdaInfo<LambdaType>::ReturnType;

template<typename LambdaType>
using LambdaArgTypes_T = typename TVoxelLambdaInfo<LambdaType>::ArgTypes;

template<typename LambdaType>
using LambdaSignature_T = typename TVoxelLambdaInfo<LambdaType>::Signature;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename LambdaType, typename Signature>
constexpr bool LambdaHasSignature_V =
	std::is_same_v<LambdaReturnType_T<LambdaType>, FunctionReturnType_T<Signature>> &&
	std::is_same_v<LambdaArgTypes_T<LambdaType>, FunctionArgTypes_T<Signature>>;

template<typename LambdaType, typename Type>
using LambdaDependentType_T = std::conditional_t<std::is_same_v<LambdaType, void>, void, Type>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Don't allow converting EForceInit to int32
template<typename T>
struct TConvertibleOnlyTo
{
	template<typename S>
	requires std::is_same_v<T, S>
	operator S() const
	{
		return S{};
	}
};

template<typename T>
static constexpr bool IsForceInitializable_V = std::is_constructible_v<T, TConvertibleOnlyTo<EForceInit>>;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

template<typename T>
static constexpr bool CanMakeSafe =
	!TIsTSharedRef_V<T> &&
	!std::derived_from<T, UObject> &&
	(std::is_constructible_v<T> || IsForceInitializable_V<T>);

template<typename T>
requires
(
	CanMakeSafe<T> &&
	!IsForceInitializable_V<T>
)
T MakeSafe()
{
	return T{};
}

template<typename T>
requires
(
	CanMakeSafe<T> &&
	IsForceInitializable_V<T>
)
T MakeSafe()
{
	return T(ForceInit);
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

template<typename>
struct TMakeWeakPtrLambdaHelper;

template<typename... ArgTypes>
struct TMakeWeakPtrLambdaHelper<TVoxelTypes<ArgTypes...>>
{
	template<typename T, typename LambdaType>
	FORCEINLINE static auto Make(const T& Ptr, LambdaType&& Lambda)
	{
		return [WeakPtr = MakeWeakPtr(Ptr), Lambda = MoveTemp(Lambda)](ArgTypes... Args)
		{
			if (const auto Pinned = WeakPtr.Pin())
			{
				Lambda(Forward<ArgTypes>(Args)...);
			}
		};
	}
	template<typename T, typename LambdaType, typename ReturnType>
	FORCEINLINE static auto Make(const T& Ptr, LambdaType&& Lambda, ReturnType&& Default)
	{
		return [WeakPtr = MakeWeakPtr(Ptr), Lambda = MoveTemp(Lambda), Default = MoveTemp(Default)](ArgTypes... Args) -> ReturnType
		{
			if (const auto Pinned = WeakPtr.Pin())
			{
				return Lambda(Forward<ArgTypes>(Args)...);
			}
			else
			{
				return Default;
			}
		};
	}
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

template<typename T, typename = decltype(std::declval<T>().AsWeak())>
FORCEINLINE TWeakPtr<T> MakeWeakPtr(T* Ptr)
{
	return StaticCastWeakPtr<T>(Ptr->AsWeak());
}
template<typename T, typename = decltype(std::declval<T>().AsWeak())>
FORCEINLINE TWeakPtr<T> MakeWeakPtr(T& Ptr)
{
	return StaticCastWeakPtr<T>(Ptr.AsWeak());
}

template<typename T, typename = decltype(std::declval<T>().AsShared())>
FORCEINLINE TSharedRef<T> MakeSharedRef(T* Ptr)
{
	return StaticCastSharedRef<T>(Ptr->AsShared());
}
template<typename T, typename = decltype(std::declval<T>().AsShared())>
FORCEINLINE TSharedRef<T> MakeSharedRef(T& Ptr)
{
	return StaticCastSharedRef<T>(Ptr.AsShared());
}
template<typename T>
FORCEINLINE TSharedRef<T> MakeSharedRef(const TSharedRef<T>& Ref)
{
	return Ref;
}

template<typename T>
FORCEINLINE TWeakPtr<T> MakeWeakPtr(const TSharedPtr<T>& Ptr)
{
	return TWeakPtr<T>(Ptr);
}
template<typename T>
FORCEINLINE TWeakPtr<T> MakeWeakPtr(const TSharedRef<T>& Ptr)
{
	return TWeakPtr<T>(Ptr);
}

template<typename T>
concept CanMakeWeakPtr = requires(T Value)
{
	MakeWeakPtr(Value);
};

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

template<typename T, typename LambdaType>
requires
(
	std::is_void_v<LambdaReturnType_T<LambdaType>> &&
	CanMakeWeakPtr<T>
)
FORCEINLINE auto MakeWeakPtrLambda(const T& Ptr, LambdaType Lambda)
{
	return TMakeWeakPtrLambdaHelper<LambdaArgTypes_T<LambdaType>>::Make(Ptr, MoveTemp(Lambda));
}

template<typename T, typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
requires
(
	!std::is_void_v<ReturnType> &&
	CanMakeSafe<ReturnType> &&
	CanMakeWeakPtr<T>
)
FORCEINLINE auto MakeWeakPtrLambda(const T& Ptr, LambdaType Lambda)
{
	return TMakeWeakPtrLambdaHelper<LambdaArgTypes_T<LambdaType>>::Make(
		Ptr,
		MoveTemp(Lambda),
		MakeSafe<ReturnType>());
}

template<typename T, typename LambdaType, typename ReturnType = LambdaReturnType_T<LambdaType>>
requires (!std::is_void_v<ReturnType>)
FORCEINLINE auto MakeWeakPtrLambda(const T& Ptr, LambdaType Lambda, ReturnType&& Default)
{
	return TMakeWeakPtrLambdaHelper<LambdaArgTypes_T<LambdaType>>::Make(
		Ptr,
		MoveTemp(Lambda),
		MoveTemp(Default));
}

template<typename LambdaType>
FORCEINLINE auto MakeLambdaDelegate(LambdaType Lambda)
{
	return TDelegate<LambdaSignature_T<LambdaType>>::CreateLambda(MoveTemp(Lambda));
}

template<typename T, typename LambdaType>
requires
(
	std::derived_from<T, UObject> ||
	std::derived_from<T, IInterface>
)
FORCEINLINE auto MakeWeakObjectPtrDelegate(T* Ptr, LambdaType Lambda)
{
	return TDelegate<LambdaSignature_T<LambdaType>>::CreateWeakLambda(Ptr, MoveTemp(Lambda));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// Need requires as &&& is equivalent to &, so T could get matched with Smthg&
template<typename T>
requires (!std::is_reference_v<T>)
FORCEINLINE TSharedRef<T> MakeSharedCopy(T&& Data)
{
	return MakeShared<T>(MoveTemp(Data));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<typename T>
FProperty& FindFPropertyChecked_Impl(const FName Name)
{
	UStruct* Struct;
	if constexpr (std::derived_from<T, UObject>)
	{
		Struct = T::StaticClass();
	}
	else
	{
		Struct = T::StaticStruct();
	}

	FProperty* Property = FindFProperty<FProperty>(Struct, Name);
	check(Property);
	return *Property;
}

#define FindFPropertyChecked_ByName(Class, Name) \
	([]() -> FProperty& \
	{ \
		static FProperty& Property = FindFPropertyChecked_Impl<Class>(Name); \
		return Property; \
	}())

#define FindFPropertyChecked(Class, Name) FindFPropertyChecked_ByName(Class, GET_MEMBER_NAME_CHECKED(Class, Name))