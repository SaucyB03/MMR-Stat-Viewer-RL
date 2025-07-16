#include "pch.h"

#include "MMR_Display.h"

void MMR_Display::RenderSettings() {
	CVarWrapper enabled = cvarManager->getCvar("enableMMRStatDisplay");
	bool enabler = enabled.getBoolValue();

	if (ImGui::Checkbox("Enable MMR/Stats Viewer", &enabler)) {
		enabled.setValue(enabler);
		gameWrapper->Execute([this](GameWrapper* gw) {
			cvarManager->executeCommand("togglemenu " + menuTitle_);
			});
	}

	if (enabler) {
		CVarWrapper scaling = cvarManager->getCvar("MMRStatScale");
		float curScale = scaling.getFloatValue();
		if (ImGui::SliderFloat("Scale", &curScale, 5.0f, 30.0f, "%.1f")) {
			scaling.setValue(curScale);
		}
	}


	if (enabler && !gameWrapper->IsInOnlineGame()) {
		ImGui::Text("GUI will now show when you join a game.");
	}
}