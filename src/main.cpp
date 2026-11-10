#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/Popup.hpp>
#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace {
	constexpr double DEFAULT_TPS = 240.0;
	constexpr double BLOCK = 30.0;

	struct ModeInfo {
		char const* name;
		double hitbox;
		double miniHitbox;
		bool wave;
	};

	// Approximate solid hitbox sizes in units. Override with "Custom hitbox size" in settings.
	constexpr ModeInfo MODES[] = {
		{ "Cube", 30.0, 18.0, false },
		{ "Ship", 30.0, 18.0, false },
		{ "Ball", 30.0, 18.0, false },
		{ "UFO", 30.0, 18.0, false },
		{ "Wave", 10.0, 6.0, true },
		{ "Robot", 30.0, 18.0, false },
		{ "Spider", 30.0, 18.0, false },
		{ "Swing", 30.0, 18.0, false },
	};

	ModeInfo const& findMode(std::string const& name) {
		for (auto const& m : MODES) {
			if (name == m.name) return m;
		}
		return MODES[4];
	}

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

	bool visibleChain(CCNode* node) {
		for (auto n = node; n; n = n->getParent()) {
			if (!n->isVisible()) return false;
		}
		return node != nullptr;
	}

	std::string buildReport(GameObject* a, GameObject* b) {
		auto* mod = Mod::get();
		double ticks = static_cast<double>(std::max<int64_t>(1, mod->getSettingValue<int64_t>("ticks")));
		auto speedName = mod->getSettingValue<std::string>("speed");
		auto modeName = mod->getSettingValue<std::string>("gamemode");
		auto axisName = mod->getSettingValue<std::string>("axis");
		bool mini = mod->getSettingValue<bool>("mini");
		double customSpeed = mod->getSettingValue<double>("custom-speed");
		double customHitbox = mod->getSettingValue<double>("custom-hitbox");

		auto const& mode = findMode(modeName);

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

		double portal = baseSpeed(speedName);
		double speed = portal;
		std::string speedNote = speedName;
		if (customSpeed > 0.0) {
			speed = customSpeed;
			speedNote = "custom";
		} else if (!horiz) {
			if (mode.wave) {
				speed = portal * (mini ? 2.0 : 1.0);
				speedNote = mini ? speedName + ", mini wave x2" : speedName;
			} else {
				speedNote = speedName + ", vertical speed varies - set Custom speed";
			}
		}

		double player = customHitbox > 0.0 ? customHitbox : (mini ? mode.miniHitbox : mode.hitbox);
		double freeSpace = gap - player;

		std::string out;
		out += fmt::format(
			"<cy>Gap X:</c> {:.2f}u ({:.2f}b)   <cy>Gap Y:</c> {:.2f}u ({:.2f}b)\n",
			gx, gx / BLOCK, gy, gy / BLOCK
		);
		out += fmt::format(
			"<cy>Mode:</c> {}{}   <cy>Axis:</c> {}\n",
			mode.name, mini ? " (mini)" : "", horiz ? "Horizontal" : "Vertical"
		);
		out += fmt::format(
			"<cy>Speed:</c> {:.2f} u/s ({})   <cy>Hitbox:</c> {:.0f}u\n",
			speed, speedNote, player
		);
		out += fmt::format("<cy>Step per tick @ {:.0f} TPS:</c> {:.3f}u\n", DEFAULT_TPS, speed / DEFAULT_TPS);

		if (gap <= 0.0) {
			out += fmt::format(
				"\n<cr>No gap on this axis</c> - the objects {} along it. Try the other axis in settings.",
				gap == 0.0 ? "touch" : "overlap"
			);
			return out;
		}

		out += fmt::format("<cy>Free window:</c> {:.3f}u (gap - hitbox)\n\n", freeSpace);

		if (freeSpace > 0.01) {
			double minTps = speed / freeSpace;
			double recTps = speed * ticks / freeSpace;
			double ticksAtDefault = freeSpace * DEFAULT_TPS / speed;

			out += fmt::format("<cg>Minimum TPS:</c> {}  (>= 1 tick inside)\n", tpsStr(minTps));
			out += fmt::format("<cg>Recommended TPS:</c> {}  (>= {:.0f} ticks inside)\n", tpsStr(recTps), ticks);
			out += fmt::format("<cl>Ticks inside @ {:.0f} TPS:</c> {:.2f}\n", DEFAULT_TPS, ticksAtDefault);
			if (recTps <= DEFAULT_TPS) {
				out += "Safe at the default TPS.";
			} else if (minTps <= DEFAULT_TPS) {
				out += "<co>Passable at 240 TPS but not reliably - raise TPS.</c>";
			} else {
				out += "<cr>Needs a TPS above the default 240 (TPS bypass).</c>";
			}
		} else if (freeSpace >= -0.01) {
			out += "<cr>Gap equals the hitbox size.</c> Tick-perfect: no finite TPS guarantees a tick inside it.";
		} else {
			double maxClip = speed / (span + player);
			out += fmt::format(
				"<cr>Gap is {:.2f}u smaller than the hitbox.</c> The player can't sit inside it; the only way across is skipping over it in one tick.\n",
				-freeSpace
			);
			if (maxClip < 1.0) {
				out += "<co>Max TPS to clip across:</c> below 1 (not realistic)";
			} else {
				out += fmt::format("<co>Max TPS to clip across:</c> {}", static_cast<long long>(std::floor(maxClip)));
			}
		}
		return out;
	}
}

class GapPopup : public geode::Popup {
protected:
	bool init(std::string const& report) {
		if (!Popup::init(360.f, 250.f)) return false;
		this->setTitle("Gap Calculator");

		auto area = TextArea::create(report, "chatFont.fnt", 0.7f, 330.f, { 0.5f, 0.5f }, 16.f, false);
		area->setPosition({ m_size.width / 2.f, m_size.height / 2.f - 4.f });
		m_mainLayer->addChild(area);

		auto spr = ButtonSprite::create("Settings", "goldFont.fnt", "GJ_button_04.png", .6f);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GapPopup::onSettings));
		btn->setPosition({ m_size.width / 2.f, 22.f });
		m_buttonMenu->addChild(btn);

		return true;
	}

	void onSettings(CCObject*) {
		openSettingsPopup(Mod::get());
	}

public:
	static GapPopup* create(std::string const& report) {
		auto ret = new GapPopup();
		if (ret->init(report)) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

class $modify(GapEditorUI, EditorUI) {
	bool init(LevelEditorLayer* lel) {
		if (!EditorUI::init(lel)) return false;

		auto label = CCLabelBMFont::create("GAP", "bigFont.fnt");
		label->setScale(0.55f);
		auto spr = EditorButtonSprite::create(label, EditorBaseColor::DarkGray);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GapEditorUI::onGapCalc));
		btn->setID("gap-calc-button"_spr);

		auto menu = CCMenu::create();
		menu->setID("gap-calc-menu"_spr);
		menu->setVisible(false);
		menu->addChild(btn);
		this->addChild(menu, 20);

		// Follow the edit panel every frame so the button sits next to the color picker
		this->schedule(schedule_selector(GapEditorUI::updateGapBtn));
		this->updateGapBtn(0.f);

		return true;
	}

	void updateGapBtn(float) {
		auto menu = this->getChildByID("gap-calc-menu"_spr);
		if (!menu) return;

		// Row layout in the edit panel: [color picker] [Go To Layer] [Deselect]
		auto ref = m_goToLayerBtn;
		auto ref2 = m_deselectBtn;
		if (!ref || !ref2 || !ref->getParent() || !ref2->getParent() || !visibleChain(ref)) {
			menu->setVisible(false);
			return;
		}

		auto wp = ref->getParent()->convertToWorldSpace(ref->getPosition());
		auto wd = ref2->getParent()->convertToWorldSpace(ref2->getPosition());
		float stepX = wp.x - wd.x; // points from Deselect towards the color picker
		if (std::fabs(stepX) < 1.f) {
			menu->setVisible(false);
			return;
		}

		// Color picker is one step further, the Gap button goes one more step beyond it
		auto target = this->convertToNodeSpace(ccp(wp.x + 2.f * stepX, wp.y));
		auto local1 = this->convertToNodeSpace(wp);
		auto local2 = this->convertToNodeSpace(wd);
		float cell = std::fabs(local1.x - local2.x);

		auto* mod = Mod::get();
		float offX = static_cast<float>(mod->getSettingValue<double>("button-offset-x"));
		float offY = static_cast<float>(mod->getSettingValue<double>("button-offset-y"));
		menu->setPosition({ target.x + offX, target.y + offY });

		if (auto btn = menu->getChildByID("gap-calc-button"_spr)) {
			float w = btn->getContentSize().width;
			if (w > 1.f) {
				menu->setScale(std::clamp(cell * 0.9f / w, 0.2f, 2.f));
			}
		}
		menu->setVisible(true);
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

		if (auto popup = GapPopup::create(body)) popup->show();
	}
};
