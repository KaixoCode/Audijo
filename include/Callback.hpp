#pragma once
#include "pch.hpp"

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
	template<typename _Fx>
	struct LambdaSignature 
	{
		using type = typename MemberSignature<decltype(&_Fx::operator())>::type;
	};

	// Valid sample formats
	template<typename Format>
	concept ValidFormat = 
		   std::is_same_v<Format, int8_t**>
		|| std::is_same_v<Format, int16_t**>
		|| std::is_same_v<Format, int32_t**>
		|| std::is_same_v<Format, float**>
		|| std::is_same_v<Format, double**>;

	// Valid callback signature
	template<typename Ret, typename InFormat, typename OutFormat, typename CI,  typename ...UserData>
	concept ValidCallback = std::is_same_v<Ret, void>      // Return must be void
		&& ValidFormat<InFormat> && ValidFormat<OutFormat> // First 2 must be valid formats
		&& std::is_same_v<InFormat, OutFormat>
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
	using Callback = int(*)(Args...);

	/**
	 * Callback Wrapper base, for type erasure.
	 */
	struct CallbackWrapperBase
	{
		virtual void Call(void** in, void** out, CallbackInfo&& info, void* userdata) = 0;
		virtual int Bytes() = 0;
		virtual bool Floating() = 0;
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
	public:
		CallbackWrapper(Type callback)
			: m_Callback(callback)
		{}

		int Bytes() override { return sizeof(std::remove_pointer_t<std::remove_pointer_t<NthTypeOf<0, Args...>>>); }
		bool Floating() override { return std::is_floating_point_v<std::remove_pointer_t<std::remove_pointer_t<NthTypeOf<0, Args...>>>>; }

		void Call(void** in, void** out, CallbackInfo&& info, void* userdata) override
		{
			if constexpr (sizeof...(Args) == 4)
				if constexpr (std::is_reference_v<NthTypeOf<2, Args...>>)
					m_Callback(
						reinterpret_cast<NthTypeOf<0, Args...>>(in),
						reinterpret_cast<NthTypeOf<1, Args...>>(out), 
						std::forward<CallbackInfo>(info),
						*reinterpret_cast<std::remove_reference_t<NthTypeOf<3, Args...>>*>(userdata));
				else 
					m_Callback(
						reinterpret_cast<NthTypeOf<0, Args...>>(in),
						reinterpret_cast<NthTypeOf<1, Args...>>(out), 
						std::forward<CallbackInfo>(info),
						reinterpret_cast<NthTypeOf<3, Args...>>(userdata));
			else 
				m_Callback(
					reinterpret_cast<NthTypeOf<0, Args...>>(in),
					reinterpret_cast<NthTypeOf<1, Args...>>(out), 
					std::forward<CallbackInfo>(info));
		}

	private:
		Type m_Callback;
	};
}