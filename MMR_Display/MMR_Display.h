#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"
#include "bakkesmod/wrappers/GuiManagerWrapper.h"
#include <format>
#include <chrono>

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

using namespace std;

class MMR_Display: public BakkesMod::Plugin::BakkesModPlugin, public BakkesMod::Plugin::PluginSettingsWindow, public BakkesMod::Plugin::PluginWindow
{

	//std::shared_ptr<bool> enabled;
	struct StatTickerParams {
		// person who got a stat
		uintptr_t Receiver;
		// person who is victim of a stat (only exists for demos afaik)
		uintptr_t Victim;
		// wrapper for the stat event
		uintptr_t StatEvent;
	};

	Vector2F GRP_CENT;
	const float SCALE = 0.1;
	const float MARGIN = 5;
	const float TEXT_FLOAT_L_DIST = 6;
	const float TEXT_SCALE = 17.5f;
	const int ANIM_LENGTH = 5;
	const float ANIM_MOVE_TIME = 1;
	const float ANIM_FADE_TIME = 1;


	//Boilerplate
	void onLoad() override;
	//void onUnload() override; // Uncomment and implement if you need a unload method

	void loadFonts();

	void onStatTickerMessage(void* params);
	void updateStats();

	string GetPluginName();

	string GetMenuName();
	string GetMenuTitle();
	void SetImGuiContext(uintptr_t ctx);
	bool ShouldBlockInput();
	bool IsActiveOverlay();
	void OnOpen();
	void OnClose();
	void Render();


	bool isWindowOpen_ = false;
	string menuTitle_ = "Stats/MMR_Viewer";

	shared_ptr<bool> enable = make_shared<bool>();
	shared_ptr<float> scale = make_shared<float>(17.5);
	unique_ptr<MMRNotifierToken> notifierToken;
	shared_ptr<ImageWrapper> streakImage;
	shared_ptr<ImageWrapper> winImage;
	shared_ptr<ImageWrapper> lossImage;
	shared_ptr<ImageWrapper> mmrImage;

	Vector2F screenSize;
	float mmr;
	ImFont* UIFont;

	bool gameEnd = false;
	bool showDeltas = false;
	int streak = 0;
	int sessionWins = 0;
	int sessionLoss = 0;
	vector<float> deltas = { 0,0,0,0 };
	float deltaStreak = 0;

	chrono::time_point<std::chrono::steady_clock> animTime;
	chrono::time_point<std::chrono::steady_clock> lastFrameTime;

public:
	void RenderSettings() override; // Uncomment if you wanna render your own tab in the settings menu
	//void RenderWindow() override; // Uncomment if you want to render your own plugin window
};
