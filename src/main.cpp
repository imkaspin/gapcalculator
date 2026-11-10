#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
	// --- Button placement (tweak to taste) ---------------------------------
	constexpr float BTN_X_FROM_RIGHT = 30.f;
	constexpr float BTN_Y_FROM_TOP = 110.f;

	constexpr double DEFAULT_TPS = 240.0;
	constexpr double BLOCK = 30.0;

	// Player speeds in units/second for each speed portal
	double baseSpeed(std::string const& s) {
		if (s == "0.5x") return 251.1612;
		if (s == "2x") return 387.42;
		if (s == "3x") return 468.0;
		if (s == "4x") return 576.0;
		return 311.5806; // 1x
	}

	// Object bounds in editor units, with a 30x30 fallback if the rect is empty
	CCRect boundsOf(GameObject* obj) {
		CCRect r = obj->getObjectRect();
		if (r.size.width < 1.f || r.size.height < 1.f) {
			auto p = obj->getPosition();
			r = CCRect(p.x - 15.f, p.y - 15.f, 30.f, 30.f);
		}
		return r;
	}

	std::string tpsStr(double v) {
		if (!std::isfinite(v) || v > 10000000.0) return "infinite";
		return fmt::format("{}", static_cast<long long>(std::ceil(v)));
	}

	std::string buildReport(GameObject* a, GameObject* b) {
		auto* mod = Mod::get();
		double ticks = static_cast<double>(std::max<int64_t>(1, mod->getSettingValue<int64_t>("ticks")));
		auto speedName = mod->getSettingValue<std::string>("speed");
		auto sizeName = mod->getSettingValue<std::string>("player-size");
		auto axisName = mod->getSettingValue<std::string>("axis");
		double custom = mod->getSettingValue<double>("custom-speed");

		CCRect ra = boundsOf(a);
		CCRect rb = boundsOf(b);

		// Edge-to-edge gaps. Negative = the objects overlap on that axis.
		float gx = std::max(ra.getMinX(), rb.getMinX()) - std::min(ra.getMaxX(), rb.getMaxX());
		float gy = std::max(ra.getMinY(), rb.getMinY()) - std::min(ra.getMaxY(), rb.getMaxY());

		bool horiz;
		if (axisName == "Horizontal") horiz = true;
		else if (axisName == "Vertical") horiz = false;
		else horiz = (gx > 0.f) || (gy <= 0.f);

		double gap = horiz ? gx : gy;
		double span = horiz
			? std::max(ra.getMaxX(), rb.getMaxX()) - std::min(ra.getMinX(), rb.getMinX())
			: std::max(ra.getMaxY(), rb.getMaxY()) - std::min(ra.getMinY(), rb.getMinY());

		double speed = custom > 0.0 ? custom : baseSpeed(speedName);
		double player = (sizeName == "Mini") ? 18.0 : 30.0;
		double freeSpace = gap - player;

		std::string out;
		out += fmt::format("<cy>Gap X:</c> {:.2f} u ({:.2f} blocks)\n", gx, gx / BLOCK);
		out += fmt::format("<cy>Gap Y:</c> {:.2f} u ({:.2f} blocks)\n", gy, gy / BLOCK);
		out += fmt::format(
			"<cy>Axis:</c> {}  <cy>Speed:</c> {:.2f} u/s {}\n",
			horiz ? "Horizontal" : "Vertical",
			speed,
			custom > 0.0 ? std::string("(custom)") : fmt::format("({})", speedName)
		);
		out += fmt::format("<cy>Player hitbox:</c> {:.0f} u ({})\n", player, sizeName);
		out += fmt::format("<cy>Step per tick @ {:.0f} TPS:</c> {:.3f} u\n\n", DEFAULT_TPS, speed / DEFAULT_TPS);

		if (gap <= 0.0) {
			out += fmt::format(
				"<cr>No gap on this axis</c> - the objects {} along it.\n"
				"Try switching the axis in settings.",
				gap == 0.0 ? "touch" : "overlap"
			);
			return out;
		}

		out += fmt::format("<cy>Free window:</c> {:.3f} u (gap - hitbox)\n\n", freeSpace);

		if (freeSpace > 0.01) {
			double minTps = speed / freeSpace;
			double recTps = speed * ticks / freeSpace;
			double ticksAtDefault = freeSpace * DEFAULT_TPS / speed;

			out += fmt::format("<cg>Minimum TPS:</c> {}  (>= 1 tick inside the gap)\n", tpsStr(minTps));
			out += fmt::format("<cg>Recommended TPS:</c> {}  (>= {:.0f} ticks inside)\n", tpsStr(recTps), ticks);
			out += fmt::format("<cl>Ticks inside @ {:.0f} TPS:</c> {:.2f}\n", DEFAULT_TPS, ticksAtDefault);
			if (recTps <= DEFAULT_TPS) {
				out += "\nSafe at the default TPS.";
			} else if (minTps <= DEFAULT_TPS) {
				out += "\n<co>Passable at default TPS but not reliably - raise TPS for consistency.</c>";
			} else {
				out += "\n<cr>Needs a TPS above the default 240 (TPS bypass).</c>";
			}
		} else if (freeSpace >= -0.01) {
			out += "<cr>Gap equals the hitbox size.</c>\nThis is tick-perfect: no finite TPS guarantees a tick inside it.";
		} else {
			double maxClip = speed / (span + player);
			out += fmt::format(
				"<cr>Gap is smaller than the hitbox ({:.2f} u short).</c>\n"
				"The player can't sit inside it. The only way across is skipping over it in a single tick.\n\n"
				"<co>Max TPS to clip across:</c> {}\n"
				"(step per tick must be >= span {:.2f} u + hitbox {:.0f} u)",
				-freeSpace, std::to_string(static_cast<long long>(std::floor(maxClip))), span, player
			);
		}
		return out;
	}
}

class $modify(GapEditorUI, EditorUI) {
	bool init(LevelEditorLayer* lel) {
		if (!EditorUI::init(lel)) return false;

		auto winSize = CCDirector::get()->getWinSize();

		auto spr = ButtonSprite::create("Gap");
		spr->setScale(0.7f);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GapEditorUI::onGapCalc));
		btn->setID("gap-calc-button"_spr);

		auto menu = CCMenu::create();
		menu->setID("gap-calc-menu"_spr);
		menu->setPosition({ winSize.width - BTN_X_FROM_RIGHT, winSize.height - BTN_Y_FROM_TOP });
		menu->addChild(btn);
		this->addChild(menu, 10);

		return true;
	}

	void onGapCalc(CCObject*) {
		auto objs = this->getSelectedObjects();
		GameObject* a = nullptr;
		GameObject* b = nullptr;
		if (objs && objs->count() == 2) {
			a = typeinfo_cast<GameObject*>(objs->objectAtIndex(0));
			b = typeinfo_cast<GameObject*>(objs->objectAtIndex(1));
		}

		std::string body = (a && b)
			? buildReport(a, b)
			: "Select <cy>exactly 2 objects</c> in the editor, then press this button again.";

		createQuickPopup(
			"Gap Calculator",
			body,
			"Close", "Settings",
			380.f,
			[](FLAlertLayer*, bool settings) {
				if (settings) openSettingsPopup(Mod::get());
			}
		);
	}
};
