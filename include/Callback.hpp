#pragma once
#include "pch.hpp"
#include "Buffer.hpp"

namespace Audijo
{
	/**
	 * Information given inside of the callback.
	 */
	struct CallbackInfo
	{
		/**
		 * Amount of input channels.
		 */
		int inputChannels;

		/**
		 * Amount of output channels.
		 */
		int outputChannels;

		/**
		 * Buffer size
		 */
		int bufferSize;

		/**
		 * Sample rate
		 */
		double sampleRate;
	};

	// Get elements from template packs.
	template<int N, typename... Ts> using NthTypeOf =
		typename std::tuple_element<N, std::tuple<Ts...>>::type;

	// Get the signature of a member function
	template<typename T>
	struct MemberSignature {};
	template<typename Ret, typename Type, typename... Args>
	struct MemberSignature<Ret(Type::*)(Args...) const>
	{
		using type = Ret(Args...);
	};

	// Get the signature of a lambda's operator()
	template<typename _Fx, typename = void>
	struct LambdaSignature;
	template<typename _Fx>
	struct LambdaSignature<_Fx, std::void_t<decltype(&_Fx::operator())>>
	{
		using type = typename MemberSignature<decltype(&_Fx::operator())>::type;
	};

	template<typename T>
	struct RemoveAllPointers : std::conditional_t<std::is_pointer_v<T>,
		RemoveAllPointers<std::remove_pointer_t<T>>,std::type_identity<T>>
	{};

	template<typename T>
	using RemoveAllPointersT = typename RemoveAllPointers<T>::type;

	// Valid sample formats
	template<typename Format>
	concept ValidFormat = 
		   std::is_same_v<Format, int8_t**>
		|| std::is_same_v<Format, int16_t**>
		|| std::is_same_v<Format, int32_t**>
		|| std::is_same_v<Format, float**>
		|| std::is_same_v<Format, double**>
		|| std::is_same_v<Format, Buffer<int8_t>&>
		|| std::is_same_v<Format, Buffer<int16_t>&>
		|| std::is_same_v<Format, Buffer<int32_t>&>
		|| std::is_same_v<Format, Buffer<float>&>
		|| std::is_same_v<Format, Buffer<double>&>;

	// Valid callback signature
	template<typename Ret, typename InFormat, typename OutFormat, typename CI,  typename ...UserData>
	concept ValidCallback = std::is_same_v<Ret, void>      // Return must be void
		&& ValidFormat<InFormat> && ValidFormat<OutFormat> // First 2 must be valid formats
		&& std::is_same_v<CI, CallbackInfo>
		&& sizeof...(UserData) <= 1 && ((std::is_reference_v<UserData> && ...) 
			|| (std::is_pointer_v<UserData> && ...));      // userdata is optional, must be reference or pointer

	// Signature check for lambdas
	template<typename T>
	constexpr static bool SignatureCheck = false;
	template<typename Ret, typename ...Args>
	constexpr static bool SignatureCheck<Ret(Args...)> = ValidCallback<Ret, Args...>;
	template<typename Signature>
	concept LambdaConstraint = SignatureCheck<typename LambdaSignature<Signature>::type>;

	// Callback type
	template<typename ...Args> requires ValidCallback<int, Args...>
	using Callback = void(*)(Args...);

	/**
	 * Callback Wrapper base, for type erasure.
	 */
	struct CallbackWrapperBase
	{
		virtual void Call(void** in, void** out, CallbackInfo&& info, void* userdata) = 0;
		virtual int InFormat() = 0;
		virtual int OutFormat() = 0;
	};

	template<typename, typename>
	class CallbackWrapper;

	/**
	 * Typed callback wrapper, implements pure virtual Call method and casts data back to
	 * original type.
	 */
	template<typename Type, typename ...Args> requires ValidCallback<void, Args...>
	class CallbackWrapper<Type, void(Args...)> : public CallbackWrapperBase
	{
		template<typename T>
		struct IsFloat
		{
			constexpr static bool value = std::is_floating_point_v<T>;
		};

		template<typename T>
		struct IsFloat<Buffer<T>> : IsFloat<T>
		{};

		template<typename T>
		struct TypeSize
		{
			constexpr static int value = sizeof(T);
		};

		template<typename T>
		struct TypeSize<Buffer<T>> : TypeSize<T>
		{};

	public:

		using InType = std::remove_reference_t<NthTypeOf<0, Args...>>;
		using OutType = std::remove_reference_t<NthTypeOf<1, Args...>>;

		constexpr static bool InFloat = IsFloat<RemoveAllPointersT<InType>>::value;
		constexpr static bool OutFloat = IsFloat<RemoveAllPointersT<OutType>>::value;
		constexpr static int InSize = TypeSize<RemoveAllPointersT<InType>>::value;
		constexpr static int OutSize = TypeSize<RemoveAllPointersT<OutType>>::value;

		CallbackWrapper(Type callback)
			: m_Callback(callback)
		{}

		int InFormat() override { return (InFloat ? 0x10 : 0x00) | InSize; }
		int OutFormat() override { return (OutFloat ? 0x10 : 0x00) | OutSize; }

		void Call(void** in, void** out, CallbackInfo&& info, void* userdata) override
		{
			InType _in;
			OutType _out;
			if constexpr (std::is_class_v<InType>)
				_in = InType{ reinterpret_cast<InType::Type**>(in), info.inputChannels, info.bufferSize };
			else
				_in = reinterpret_cast<InType>(in);

			if constexpr (std::is_class_v<OutType>)
				_out = OutType{ reinterpret_cast<OutType::Type**>(out), info.outputChannels, info.bufferSize };
			else
				_out = reinterpret_cast<OutType>(out);

			if constexpr (sizeof...(Args) == 4)
				if constexpr (std::is_reference_v<NthTypeOf<3, Args...>>)
					m_Callback(
						_in,
						_out, 
						std::forward<CallbackInfo>(info),
						*reinterpret_cast<std::remove_reference_t<NthTypeOf<3, Args...>>*>(userdata));
				else 
					m_Callback(
						_in,
						_out,
						std::forward<CallbackInfo>(info),
						reinterpret_cast<NthTypeOf<3, Args...>>(userdata));
			else 
				m_Callback(
					_in,
					_out,
					std::forward<CallbackInfo>(info));
		}

	private:
		Type m_Callback;
	};
}