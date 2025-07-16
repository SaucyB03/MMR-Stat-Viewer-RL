#include "pch.h"
#include "MMR_Display.h"


BAKKESMOD_PLUGIN(MMR_Display, "write a plugin description here", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void MMR_Display::onLoad()
{
	_globalCvarManager = cvarManager;

	string res = gameWrapper->GetSettings().GetVideoSettings().Resolution;
	size_t x_pos = res.find('x');
	screenSize.X = stoi(res.substr(0, x_pos));
	screenSize.Y = stoi(res.substr(x_pos + 1));
	GRP_CENT = { screenSize.X * 0.75f,screenSize.Y * 0.85f };

	cvarManager->registerCvar("enableMMRStatDisplay", "0", "Enables the plugin", true, true, 0, true, 1, true).bindTo(enable);
	cvarManager->registerCvar("MMRStatScale", "17.5", "Scale of UI", true, true, 5.0, true, 30.0, true).bindTo(scale);



	// load images:
	streakImage = make_shared<ImageWrapper>(gameWrapper->GetDataFolder() / "MMR_Display" / "streak-RL-icon.png", false, true);
	winImage = std::make_shared<ImageWrapper>(gameWrapper->GetDataFolder() / "MMR_Display" / "win-RL-icon.png", false, true);
	lossImage = std::make_shared<ImageWrapper>(gameWrapper->GetDataFolder() / "MMR_Display" / "loss-RL-icon.png", false, true);
	mmrImage = std::make_shared<ImageWrapper>(gameWrapper->GetDataFolder() / "MMR_Display" / "mmr-RL-icon.png", false, true);


	notifierToken = gameWrapper->GetMMRWrapper().RegisterMMRNotifier(
		[this](UniqueIDWrapper id) {
			updateStats();
			if (isWindowOpen_ != *enable) {
				gameWrapper->Execute([this](GameWrapper* gw) {
					cvarManager->executeCommand("togglemenu " + menuTitle_);
					});
			}
			if (gameEnd == true) {
				showDeltas = true;
				gameEnd = false;
				animTime = chrono::steady_clock::now();
			}
		}
	);


	gameWrapper->HookEventWithCallerPost<ServerWrapper>("Function TAGame.GFxHUD_TA.HandleStatTickerMessage",
		[this](ServerWrapper caller, void* params, std::string eventname) {
			onStatTickerMessage(params);
		});


	loadFonts();
}

void MMR_Display::loadFonts() {
	LOG("TRYING TO LOAD FONTS......");
	GuiManagerWrapper gui = gameWrapper->GetGUIManager();
	string file = "UAV-OSD-Sans-Mono.ttf";
	auto [res, font] = gui.LoadFont("UAV-OSD-Sans-Mono", file, 200.0f);
	if (res == 0) {
		LOG("FAIL");
	}
	else if (res == 1) {
		LOG("Font Loading...");
	}
	if (font) {
		LOG("Font Successfully Loaded.");
		UIFont = gui.GetFont("UAV-OSD-Sans-Mono");
	}
}

void MMR_Display::onStatTickerMessage(void* params) {
	StatTickerParams* pStruct = (StatTickerParams*)params;
	PriWrapper receiver = PriWrapper(pStruct->Receiver);
	StatEventWrapper statEvent = StatEventWrapper(pStruct->StatEvent);
	PlayerControllerWrapper playerController = gameWrapper->GetPlayerController();
	if (!playerController) { LOG("Null controller"); return; }
	PriWrapper playerPRI = playerController.GetPRI();
	if (!playerPRI) { LOG("Null player PRI"); return; }

	if(statEvent.GetEventName() == "Win" && !gameEnd){
		
		if (playerPRI.memory_address == receiver.memory_address || playerPRI.GetTeamNum() == receiver.GetTeamNum()) {
			//Local player won! :)
			streak++;
			sessionWins++;
			deltas.at(0) = 1;
			deltas.at(1) = 1;
		}
		else {
			//Local Player lost :(
			deltas.at(0) = -streak;
			deltas.at(2) = 1;
			streak = 0;
			sessionLoss++;

		}
		gameEnd = true;
	}
}

void MMR_Display::updateStats() {
	//Retrieves mmr info for local player in the current mode
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server) return;
	GameSettingPlaylistWrapper playlist = server.GetPlaylist();
	if (!playlist) return;

	int playlistID = playlist.GetPlaylistId();

	
	PlayerControllerWrapper pc = gameWrapper->GetPlayerController();
	if (!pc) { return; }
	PriWrapper pri = pc.GetPRI();
	if (!pri) { return; }
	UniqueIDWrapper primaryID = pri.GetUniqueIdWrapper();

	float cur_mmr = gameWrapper->GetMMRWrapper().GetPlayerMMR(primaryID, playlistID);
	if (cur_mmr != mmr) {
		deltas.at(3) = cur_mmr - mmr;
		mmr = cur_mmr;
	}
}

string MMR_Display::GetPluginName()
{
	return "Stats/MMR_Viewer";
}

void MMR_Display::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

string MMR_Display::GetMenuName()
{
	return "Stats/MMR_Viewer";
}

string MMR_Display::GetMenuTitle()
{
	return menuTitle_;
}

bool MMR_Display::ShouldBlockInput()
{
	return ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;;
}

bool MMR_Display::IsActiveOverlay()
{
	return false;
}

void MMR_Display::OnOpen()
{
	isWindowOpen_ = true;
}

void MMR_Display::OnClose()
{
	isWindowOpen_ = false;
}

void MMR_Display::Render()
{	



	//Won't display menu unless in an online game
	//if (!gameWrapper->IsInOnlineGame()) {
	//	if (isWindowOpen_) {
	//		ImGui::End();
	//	}
	//	return;
	//}

	if (!UIFont) {
		loadFonts();
	}


	ImGui::SetNextWindowPos({ 0,0 });
	ImGui::SetNextWindowSize({ screenSize.X,screenSize.Y });

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs
		| ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBackground;
	// If moveOverlay is true, the user can move the overlay 
	//  When false the overlay window no longer accepts input
	// I find this useful to connect to a CVar and checkbox in the plugin's settings
	//  You can allow a user to move the overlay only when they want

	if (!ImGui::Begin(menuTitle_.c_str(), &isWindowOpen_, WindowFlags))
	{
		ImGui::End();
		return;
	}

	// Determine delta time for smooth / consistent animations:

	chrono::duration<double> deltaTime;
	auto curTime = chrono::steady_clock::now();

	float animOffPerc = 0;
	float fadePerc = 1;
	//float curTime = 0;

	if (showDeltas) {
		chrono::duration<double> elapsed_seconds = curTime - animTime;
		if (elapsed_seconds > chrono::seconds(ANIM_LENGTH)) {
			showDeltas = false;
			deltas = { 0,0,0,0 };
		}
		else {
			//auto elapsed_ms = chrono::duration_cast<std::chrono::milliseconds>(elapsed_seconds);
			animOffPerc = elapsed_seconds.count() / ANIM_MOVE_TIME;
			if (animOffPerc > 1) {
				animOffPerc = 1;
			}

			fadePerc = 1 - ((elapsed_seconds.count()- 4) / ANIM_FADE_TIME);
			if (fadePerc > 1) {
				fadePerc = 1;
			}
		} 
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImGuiIO& io = ImGui::GetIO();

	string strkTxt = to_string(streak) + "w";
	string winTxt = to_string(sessionWins);
	string lossTxt = to_string(sessionLoss);
	string mmrTxt = format("{:.0f}",mmr);


	//Display UI Images:
	auto sTex = streakImage->GetImGuiTex();
	if (streakImage->IsLoadedForImGui() && sTex) {
		Vector2F rect = streakImage->GetSizeF();
		Vector2F localCenter = { GRP_CENT.X, GRP_CENT.Y - (2 * rect.Y *SCALE + MARGIN*1.5) };
		drawList->AddImage(sTex, ImVec2(localCenter.X, localCenter.Y), ImVec2(localCenter.X + rect.X*SCALE, localCenter.Y + rect.Y * SCALE), ImVec2(0.0, 0.0), ImVec2(1, 1));
		
		// Draw associated text:
		drawList->AddText(UIFont, *scale, ImVec2(GRP_CENT.X - *scale * 4.75, localCenter.Y + (rect.Y * SCALE / 2) - *scale/2), IM_COL32(255, 255, 255, 255), strkTxt.c_str());

		//Draw delta text:
		if (showDeltas && deltas.at(0) != 0) {
			ImU32 col = IM_COL32(0, 255, 0, 255 * fadePerc);
			string deltaTxt = "+" + format("{:.0f}", deltas.at(0));
			if (deltas.at(0) < 0) {
				deltaTxt = format("{:.0f}", deltas.at(0));
				col = IM_COL32(255, 0, 0, 255 * fadePerc);
			}
			drawList->AddText(UIFont, *scale, ImVec2(GRP_CENT.X - *scale * 4 + (*scale * animOffPerc), localCenter.Y + (rect.Y * SCALE / 2) - *scale / 2), col, deltaTxt.c_str());
		}

	}
	auto wTex = winImage->GetImGuiTex();
	if (winImage->IsLoadedForImGui() && wTex) {
		Vector2F rect = winImage->GetSizeF();
		Vector2F localCenter = { GRP_CENT.X, GRP_CENT.Y - (rect.Y * SCALE + MARGIN/2) };
		drawList->AddImage(wTex, ImVec2(localCenter.X, localCenter.Y), ImVec2(localCenter.X + rect.X * SCALE, localCenter.Y + rect.Y * SCALE), ImVec2(0.0, 0.0), ImVec2(1, 1));
		
		// Draw associated text:
		drawList->AddText(UIFont, *scale, ImVec2(GRP_CENT.X - *scale * 4, localCenter.Y + (rect.Y * SCALE / 2) - *scale / 2), IM_COL32(255, 255, 255, 255), winTxt.c_str());

		//Draw delta text:
		if (showDeltas && deltas.at(1) != 0) {
			ImU32 col = IM_COL32(0, 255, 0, 255 * fadePerc);
			string deltaTxt = "+" + format("{:.0f}", deltas.at(1));
			if (deltas.at(1) < 0) {
				deltaTxt = format("{:.0f}", deltas.at(1));
				col = IM_COL32(255, 0, 0, 255 * fadePerc);
			}

			drawList->AddText(UIFont, *scale, ImVec2(GRP_CENT.X - *scale * 4 + (*scale * animOffPerc), localCenter.Y + (rect.Y * SCALE / 2) - *scale / 2), col, deltaTxt.c_str());
		}
	}
	auto lTex = lossImage->GetImGuiTex();
	if (lossImage->IsLoadedForImGui() && lTex) {
		Vector2F rect = lossImage->GetSizeF();
		Vector2F localCenter = { GRP_CENT.X, GRP_CENT.Y + MARGIN/2};
		drawList->AddImage(lTex, ImVec2(localCenter.X, localCenter.Y), ImVec2(localCenter.X + rect.X * SCALE, localCenter.Y + rect.Y * SCALE), ImVec2(0.0, 0.0), ImVec2(1, 1));
		
		// Draw associated text:
		drawList->AddText(UIFont, *scale, ImVec2(GRP_CENT.X - *scale * 4, localCenter.Y + (rect.Y * SCALE / 2) - *scale / 2), IM_COL32(255, 255, 255, 255), lossTxt.c_str());

		//Draw delta text:
		if (showDeltas && deltas.at(2) != 0) {
			ImU32 col = IM_COL32(255, 0, 0, 255 * fadePerc);
			string deltaTxt = "+" + format("{:.0f}", deltas.at(2));
			if (deltas.at(2) < 0) {
				deltaTxt = format("{:.0f}", deltas.at(2));
				col = IM_COL32(0, 255, 0, 255 * fadePerc);
			}

			drawList->AddText(UIFont, *scale, ImVec2(GRP_CENT.X - *scale * 4 + (*scale * animOffPerc), localCenter.Y + (rect.Y * SCALE / 2) - *scale / 2), col, deltaTxt.c_str());
		}
	}
	auto mTex = mmrImage->GetImGuiTex();
	if (mmrImage->IsLoadedForImGui() && mTex) {
		Vector2F rect = mmrImage->GetSizeF();
		Vector2F localCenter = { GRP_CENT.X, GRP_CENT.Y + (rect.Y * SCALE + MARGIN * 1.5) };
		drawList->AddImage(mTex, ImVec2(localCenter.X, localCenter.Y), ImVec2(localCenter.X + rect.X * SCALE, localCenter.Y + rect.Y * SCALE), ImVec2(0.0, 0.0), ImVec2(1, 1));

		// Draw associated text:
		drawList->AddText(UIFont, *scale, ImVec2(GRP_CENT.X - *scale * 6.25, localCenter.Y + (rect.Y * SCALE / 2) - *scale / 2), IM_COL32(255, 255, 255, 255), mmrTxt.c_str());

		//Draw delta text:
		if (showDeltas && deltas.at(3) != 0) {
			ImU32 col = IM_COL32(0, 255, 0, 255 * fadePerc);
			string deltaTxt = "+" + format("{:.0f}", deltas.at(3));
			if (deltas.at(3) < 0) {
				deltaTxt = format("{:.0f}", deltas.at(3));
				col = IM_COL32(255, 0, 0, 255 * fadePerc);
			}

			drawList->AddText(UIFont, *scale, ImVec2(GRP_CENT.X - *scale*4 + (*scale * animOffPerc), localCenter.Y + (rect.Y * SCALE / 2) - *scale / 2), col, deltaTxt.c_str());
		}
	}

	ImGui::End();


	lastFrameTime = chrono::steady_clock::now();
}