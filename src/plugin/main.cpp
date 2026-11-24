#include "Body.h"
#include "ClassicCum.h"
#include "Manager.h"
#include "Menu.h"
#include "Papyrus.h"
#include "Settings.h"
#include "SexLabAlpha.h"
#include "Skee.h"

namespace
{
	void InitializeLogging()
	{
		auto path = logger::log_directory();
		if (!path) {
			SKSE::stl::report_and_fail("Unable to lookup SKSE logs directory.");
		}
		*path /= "Afterglow.log";

		auto log = std::make_shared<spdlog::logger>("Global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
		using namespace AG;
		switch (a_message->type) {
		case SKSE::MessagingInterface::kPostPostLoad:
			Settings::Get().Load();
			if (Settings::Get().debugLog) {
				spdlog::default_logger()->set_level(spdlog::level::debug);
			}
			Skee::Init();
			Menu::Register();
			break;
		case SKSE::MessagingInterface::kDataLoaded:
			Manager::Start();
			ClassicCum::Register();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Body::ClearCache();
			SexLabAlpha::Sync();
			ClassicCum::Sync();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	InitializeLogging();
	const auto* plugin = SKSE::PluginDeclaration::GetSingleton();
	logger::info("{} {} is loading", plugin->GetName(), plugin->GetVersion().string());

	SKSE::Init(a_skse, false);  // we own the logger

	if (!SKSE::GetMessagingInterface()->RegisterListener(OnMessage)) {
		SKSE::stl::report_and_fail("Unable to register the SKSE message listener.");
	}
	AG::Papyrus::Init();

	auto* ser = SKSE::GetSerializationInterface();
	ser->SetUniqueID('AGL2');
	ser->SetSaveCallback(AG::Manager::Save);
	ser->SetLoadCallback(AG::Manager::Load);
	ser->SetRevertCallback(AG::Manager::Revert);

	logger::info("{} loaded", plugin->GetName());
	return true;
}
