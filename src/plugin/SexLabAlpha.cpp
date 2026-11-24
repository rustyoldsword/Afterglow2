#include "SexLabAlpha.h"

#include "Settings.h"

namespace AG::SexLabAlpha
{
	namespace
	{
		constexpr auto kSetting = "fCumAlpha";

		class OnValue : public RE::BSScript::IStackCallbackFunctor
		{
		public:
			explicit OnValue(std::function<void(float)> a_fn) :
				_fn(std::move(a_fn)) {}
			void operator()(RE::BSScript::Variable a_result) override
			{
				if (a_result.IsFloat()) {
					_fn(a_result.GetFloat());
				}
			}
			void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

		private:
			std::function<void(float)> _fn;
		};

		// The VM does not check that a global function exists before a static call: calling one the
		// script lacks dereferences null and crashes. SexLab Classic has sslSystemConfig without
		// GetSettingFlt/SetSettingFlt, so look the function up in the loaded type first.
		bool HasGlobal(RE::BSScript::Internal::VirtualMachine* a_vm, std::string_view a_class, std::string_view a_fn)
		{
			RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
			if (!a_vm->GetScriptObjectType1(RE::BSFixedString(a_class), type) || !type) {
				return false;
			}
			const auto* funcs = type->GetGlobalFuncIter();
			for (std::uint32_t i = 0; funcs && i < type->GetNumGlobalFuncs(); ++i) {
				const auto& fn = funcs[i].func;
				if (fn && std::string_view(fn->GetName().c_str()) == a_fn) {
					return true;
				}
			}
			return false;
		}

		void SetValue(float a_value)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm || !HasGlobal(vm, "sslSystemConfig", "SetSettingFlt")) {
				return;
			}
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none;
			auto*                                                     args = RE::MakeFunctionArguments(RE::BSFixedString(kSetting), static_cast<float>(a_value));
			if (vm->DispatchStaticCall("sslSystemConfig", "SetSettingFlt", args, none)) {
				logger::info("SexLab P+ {} set to {:.2f}", kSetting, a_value);
			}
		}
	}

	Kind Detect()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!dh || !vm || !dh->LookupLoadedModByName("SexLab.esm")) {
			return Kind::None;
		}
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
		if (!vm->GetScriptObjectType1(RE::BSFixedString("sslSystemConfig"), type) || !type) {
			return Kind::None;
		}
		return HasGlobal(vm, "sslSystemConfig", "GetSettingFlt") ? Kind::Plus : Kind::Classic;
	}

	void Sync()
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			return;
		}
		if (!HasGlobal(vm, "sslSystemConfig", "GetSettingFlt")) {
			logger::info("SexLab P+ not found (no sslSystemConfig.GetSettingFlt), its overlays are left alone");
			return;
		}
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> cb(new OnValue([](float a_current) {
			auto& s = Settings::Get();
			if (s.hideSexLabOverlays) {
				if (a_current > 0.001f) {
					s.sexLabAlphaBefore = a_current;
					s.Save();
					SetValue(0.0f);
				}
			} else if (s.sexLabAlphaBefore >= 0.0f) {
				const float restore = s.sexLabAlphaBefore;
				s.sexLabAlphaBefore = -1.0f;
				s.Save();
				SetValue(restore);
			}
		}));
		auto* args = RE::MakeFunctionArguments(RE::BSFixedString(kSetting));
		if (!vm->DispatchStaticCall("sslSystemConfig", "GetSettingFlt", args, cb)) {
			logger::info("SexLab P+ not found (sslSystemConfig.GetSettingFlt), its overlays are left alone");
		}
	}
}
