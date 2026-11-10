#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/Notification.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
	constexpr double SPEEDS[] = { 251.16, 311.58, 387.42, 468.0, 576.0 };
	constexpr char const* SPEED_NAMES[] = { "0.5x", "1x", "2x", "3x", "4x" };
	constexpr char const* SIZE_NAMES[] = { "Big", "Mini", "Smallest" };
	constexpr char const* MODE_NAMES[] = { "Wave", "Cube", "Ship", "Ball", "UFO", "Robot", "Spider", "Swing" };
	constexpr int MODE_COUNT = 8;
	constexpr char const* AXIS_NAMES[] = { "Auto", "Horiz", "Vert" };

	// Solid hitbox in units. Wave values come from settings, other modes use 30 / 18.
	double hitboxFor(int mode, int size) {
		auto* mod = Mod::get();
		if (mode == 0) {
			if (size == 0) return mod->getSettingValue<double>("hitbox-big");
			if (size == 1) return mod->getSettingValue<double>("hitbox-mini");
			return mod->getSettingValue<double>("hitbox-smallest");
		}
		return size == 0 ? 30.0 : 18.0;
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

	// Edge-to-edge gaps. Negative = the rects overlap on that axis.
	float gapX(CCRect const& a, CCRect const& b) {
		return std::max(a.getMinX(), b.getMinX()) - std::min(a.getMaxX(), b.getMaxX());
	}
	float gapY(CCRect const& a, CCRect const& b) {
		return std::max(a.getMinY(), b.getMinY()) - std::min(a.getMaxY(), b.getMaxY());
	}

	bool useHorizontal(int axisSel, float gx, float gy) {
		if (axisSel == 1) return true;
		if (axisSel == 2) return false;
		return (gx > 0.f) || (gy <= 0.f);
	}

	std::string tpsStr(double v) {
		if (!std::isfinite(v) || v > 10000000.0) return "inf";
		return fmt::format("{}", static_cast<long long>(std::ceil(v)));
	}
}

class GapPopup : public geode::Popup {
protected:
	Ref<EditorUI> m_editor;
	Ref<GameObject> m_objA;
	Ref<GameObject> m_objB;
	CCRect m_ra;
	CCRect m_rb;
	bool m_hasObjs = false;

	int m_size = 1;
	int m_speed = 1;
	int m_mode = 0;
	int m_axis = 0;
	bool m_rec = true;
	double m_lastRecTps = 0.0;

	TextInput* m_input = nullptr;
	std::vector<CCMenuItemSpriteExtra*> m_sizeBtns;
	std::vector<CCMenuItemSpriteExtra*> m_speedBtns;
	CCMenuItemSpriteExtra* m_recBtn = nullptr;
	CCMenuItemSpriteExtra* m_modeBtn = nullptr;
	CCMenuItemSpriteExtra* m_axisBtn = nullptr;
	CCLabelBMFont* m_gapLabel = nullptr;
	CCLabelBMFont* m_tpsLabel = nullptr;
	CCLabelBMFont* m_minGapLabel = nullptr;
	CCLabelBMFont* m_noteLabel = nullptr;

	CCMenuItemSpriteExtra* addBtn(char const* text, float scale, SEL_MenuHandler cb, int tag = 0) {
		auto spr = ButtonSprite::create(text, "bigFont.fnt", "GJ_button_04.png", scale);
		auto item = CCMenuItemSpriteExtra::create(spr, this, cb);
		item->setTag(tag);
		m_buttonMenu->addChild(item);
		return item;
	}

	void layoutRow(std::vector<CCMenuItemSpriteExtra*> const& items, float y, float gap) {
		float total = 0.f;
		for (auto* it : items) total += it->getContentSize().width * it->getScale();
		total += gap * static_cast<float>(items.size() - 1);
		float x = (m_size.width - total) / 2.f;
		for (auto* it : items) {
			float w = it->getContentSize().width * it->getScale();
			it->setPosition({ x + w / 2.f, y });
			x += w + gap;
		}
	}

	void styleToggle(CCMenuItemSpriteExtra* item, bool selected) {
		auto spr = static_cast<ButtonSprite*>(item->getNormalImage());
		if (spr->m_BGSprite) spr->m_BGSprite->setColor(selected ? ccc3(255, 255, 255) : ccc3(70, 70, 70));
	}

	CCLabelBMFont* addLabel(char const* text, float scale, float y) {
		auto label = CCLabelBMFont::create(text, "bigFont.fnt");
		label->setScale(scale);
		label->setPosition({ m_size.width / 2.f, y });
		m_mainLayer->addChild(label);
		return label;
	}

	bool init(EditorUI* editor, GameObject* a, GameObject* b) {
		if (!Popup::init(380.f, 270.f)) return false;
		this->setTitle("Gap Calculator");

		m_editor = editor;
		if (a && b) {
			m_objA = a;
			m_objB = b;
			m_ra = boundsOf(a);
			m_rb = boundsOf(b);
			m_hasObjs = true;
		}

		auto* mod = Mod::get();
		m_size = static_cast<int>(std::clamp<int64_t>(mod->getSavedValue<int64_t>("ui-size", 1), 0, 2));
		m_speed = static_cast<int>(std::clamp<int64_t>(mod->getSavedValue<int64_t>("ui-speed", 1), 0, 4));
		m_mode = static_cast<int>(std::clamp<int64_t>(mod->getSavedValue<int64_t>("ui-mode", 0), 0, MODE_COUNT - 1));
		m_axis = static_cast<int>(std::clamp<int64_t>(mod->getSavedValue<int64_t>("ui-axis", 0), 0, 2));
		m_rec = mod->getSavedValue<int64_t>("ui-rec", 1) != 0;
		int64_t savedTps = std::max<int64_t>(1, mod->getSavedValue<int64_t>("ui-tps", 240));

		// TPS row
		auto tpsLabel = CCLabelBMFont::create("TPS / FPS:", "bigFont.fnt");
		tpsLabel->setScale(0.5f);
		tpsLabel->setPosition({ 125.f, 222.f });
		m_mainLayer->addChild(tpsLabel);

		m_input = TextInput::create(110.f, "TPS", "bigFont.fnt");
		m_input->setCommonFilter(CommonFilter::Uint);
		m_input->setMaxCharCount(6);
		m_input->setString(std::to_string(savedTps));
		m_input->setPosition({ 260.f, 222.f });
		m_input->setCallback([this](std::string const&) { this->refresh(); });
		m_mainLayer->addChild(m_input);

		// Size + Recommended row
		for (int i = 0; i < 3; i++) {
			m_sizeBtns.push_back(addBtn(SIZE_NAMES[i], 0.5f, menu_selector(GapPopup::onSize), i));
		}
		m_recBtn = addBtn("Recommended", 0.5f, menu_selector(GapPopup::onRec));
		std::vector<CCMenuItemSpriteExtra*> row1 = m_sizeBtns;
		row1.push_back(m_recBtn);
		this->layoutRow(row1, 190.f, 6.f);

		// Speed row
		for (int i = 0; i < 5; i++) {
			m_speedBtns.push_back(addBtn(SPEED_NAMES[i], 0.45f, menu_selector(GapPopup::onSpeed), i));
		}
		this->layoutRow(m_speedBtns, 160.f, 6.f);

		// Mode + Axis row
		m_modeBtn = addBtn("Mode: Wave", 0.4f, menu_selector(GapPopup::onMode));
		m_axisBtn = addBtn("Axis: Auto", 0.4f, menu_selector(GapPopup::onAxis));
		this->layoutRow({ m_modeBtn, m_axisBtn }, 130.f, 8.f);

		// Readouts
		m_gapLabel = this->addLabel("", 0.42f, 100.f);
		m_tpsLabel = this->addLabel("", 0.42f, 82.f);
		m_minGapLabel = this->addLabel("", 0.42f, 64.f);
		m_noteLabel = this->addLabel("", 0.3f, 49.f);
		m_noteLabel->setColor(ccc3(190, 190, 190));

		// Action buttons
		auto useBtn = addBtn("Use Rec TPS", 0.5f, menu_selector(GapPopup::onUseRec));
		auto setBtn = addBtn("Set Gap", 0.6f, menu_selector(GapPopup::onSetGap));
		this->layoutRow({ useBtn, setBtn }, 24.f, 10.f);

		this->refresh();
		return true;
	}

	double currentTps() {
		std::string s = m_input ? std::string(m_input->getString()) : std::string("240");
		double tps = s.empty() ? 0.0 : std::atof(s.c_str());
		return std::max(1.0, tps);
	}

	double currentSpeed() {
		double custom = Mod::get()->getSettingValue<double>("custom-speed");
		return custom > 0.0 ? custom : SPEEDS[m_speed];
	}

	double safetyTicks() {
		return static_cast<double>(std::max<int64_t>(1, Mod::get()->getSettingValue<int64_t>("ticks")));
	}

	void saveState() {
		auto* mod = Mod::get();
		mod->setSavedValue<int64_t>("ui-size", m_size);
		mod->setSavedValue<int64_t>("ui-speed", m_speed);
		mod->setSavedValue<int64_t>("ui-mode", m_mode);
		mod->setSavedValue<int64_t>("ui-axis", m_axis);
		mod->setSavedValue<int64_t>("ui-rec", m_rec ? 1 : 0);
		mod->setSavedValue<int64_t>("ui-tps", static_cast<int64_t>(currentTps()));
	}

	void refresh() {
		double speed = this->currentSpeed();
		double hitbox = hitboxFor(m_mode, m_size);
		double tps = this->currentTps();
		double ticks = this->safetyTicks();
		double step = speed / tps;
		double reqGap = hitbox + (m_rec ? ticks : 1.0) * step;

		// Button states
		for (int i = 0; i < 3; i++) this->styleToggle(m_sizeBtns[i], i == m_size);
		for (int i = 0; i < 5; i++) this->styleToggle(m_speedBtns[i], i == m_speed);
		this->styleToggle(m_recBtn, m_rec);
		static_cast<ButtonSprite*>(m_modeBtn->getNormalImage())->setString(
			fmt::format("Mode: {}", MODE_NAMES[m_mode]).c_str()
		);
		static_cast<ButtonSprite*>(m_axisBtn->getNormalImage())->setString(
			fmt::format("Axis: {}", AXIS_NAMES[m_axis]).c_str()
		);
		this->layoutRow({ m_modeBtn, m_axisBtn }, 130.f, 8.f);

		// Gap found from the selected objects -> TPS
		m_lastRecTps = 0.0;
		bool horiz = true;
		if (m_hasObjs) {
			float gx = gapX(m_ra, m_rb);
			float gy = gapY(m_ra, m_rb);
			horiz = useHorizontal(m_axis, gx, gy);
			double gap = horiz ? gx : gy;
			double freeSpace = gap - hitbox;

			m_gapLabel->setString(fmt::format("Gap: {:.4f} units ({})", gap, horiz ? "Horizontal" : "Vertical").c_str());
			m_gapLabel->setColor(ccc3(255, 255, 255));

			if (freeSpace > 0.001) {
				double minTps = speed / freeSpace;
				double recTps = speed * ticks / freeSpace;
				m_lastRecTps = recTps;
				m_tpsLabel->setString(fmt::format("Min TPS: {}   Rec TPS: {}", tpsStr(minTps), tpsStr(recTps)).c_str());
				m_tpsLabel->setColor(ccc3(255, 255, 255));
			} else if (gap <= 0.0) {
				m_tpsLabel->setString("No gap on this axis (try the other axis)");
				m_tpsLabel->setColor(ccc3(255, 110, 110));
			} else {
				m_tpsLabel->setString("Gap is too small for this hitbox");
				m_tpsLabel->setColor(ccc3(255, 110, 110));
			}
		} else {
			m_gapLabel->setString("Gap: select 2 objects first");
			m_gapLabel->setColor(ccc3(190, 190, 190));
			m_tpsLabel->setString("Min TPS: -   Rec TPS: -");
			m_tpsLabel->setColor(ccc3(190, 190, 190));
		}

		// Manual TPS -> gap
		m_minGapLabel->setString(fmt::format("{}: {:.6f} units", m_rec ? "Rec Gap" : "Min Gap", reqGap).c_str());

		if (m_mode != 0 && !horiz) {
			m_noteLabel->setString("Vertical speed varies by gamemode: portal speed used (see Custom speed)");
		} else {
			m_noteLabel->setString("");
		}

		this->saveState();
	}

	void onSize(CCObject* sender) {
		m_size = static_cast<CCNode*>(sender)->getTag();
		this->refresh();
	}

	void onSpeed(CCObject* sender) {
		m_speed = static_cast<CCNode*>(sender)->getTag();
		this->refresh();
	}

	void onRec(CCObject*) {
		m_rec = !m_rec;
		this->refresh();
	}

	void onMode(CCObject*) {
		m_mode = (m_mode + 1) % MODE_COUNT;
		this->refresh();
	}

	void onAxis(CCObject*) {
		m_axis = (m_axis + 1) % 3;
		this->refresh();
	}

	void onUseRec(CCObject*) {
		if (m_lastRecTps <= 0.0) {
			Notification::create("Select 2 objects with a valid gap first", NotificationIcon::Error)->show();
			return;
		}
		m_input->setString(tpsStr(m_lastRecTps));
		this->refresh();
	}

	void onSetGap(CCObject*) {
		if (!m_hasObjs || !m_objA || !m_objB) {
			Notification::create("Select 2 objects first", NotificationIcon::Error)->show();
			return;
		}

		double speed = this->currentSpeed();
		double hitbox = hitboxFor(m_mode, m_size);
		double reqGap = hitbox + (m_rec ? this->safetyTicks() : 1.0) * (speed / this->currentTps());

		float gx = gapX(m_ra, m_rb);
		float gy = gapY(m_ra, m_rb);
		bool horiz = useHorizontal(m_axis, gx, gy);
		double gap = horiz ? gx : gy;
		double delta = reqGap - gap;
		if (std::fabs(delta) < 1e-6) {
			Notification::create("Gap is already set", NotificationIcon::Info)->show();
			return;
		}

		// Move the object that is further right / higher up
		bool moveB = horiz ? (m_rb.getMidX() >= m_ra.getMidX()) : (m_rb.getMidY() >= m_ra.getMidY());
		GameObject* mover = moveB ? m_objB.data() : m_objA.data();
		CCPoint offset = horiz
			? ccp(static_cast<float>(delta), 0.f)
			: ccp(0.f, static_cast<float>(delta));

		auto arr = CCArray::createWithObject(mover);
		m_editor->m_editorLayer->addToUndoList(UndoObject::createWithTransformObjects(arr, UndoCommand::Transform), false);
		m_editor->moveObject(mover, offset);

		CCRect& r = moveB ? m_rb : m_ra;
		r.origin.x += offset.x;
		r.origin.y += offset.y;

		Notification::create(fmt::format("Gap set to {:.6f} units", reqGap), NotificationIcon::Success)->show();
		this->refresh();
	}

public:
	static GapPopup* create(EditorUI* editor, GameObject* a, GameObject* b) {
		auto ret = new GapPopup();
		if (ret->init(editor, a, b)) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};

class $modify(GapEditorUI, EditorUI) {
	struct Fields {
		Ref<CCMenuItemSpriteExtra> btn;
		float lastScale = 0.f;
	};

	bool init(LevelEditorLayer* lel) {
		if (!EditorUI::init(lel)) return false;

		auto label = CCLabelBMFont::create("GAP", "bigFont.fnt");
		label->setScale(0.55f);
		auto spr = EditorButtonSprite::create(label, EditorBaseColor::DarkGray);
		auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GapEditorUI::onGapCalc));
		btn->setID("gap-calc-button"_spr);
		m_fields->btn = btn;

		// The button lives inside the edit panel grid, so it only shows on the Edit tab
		this->schedule(schedule_selector(GapEditorUI::updateGapBtn));
		this->updateGapBtn(0.f);

		return true;
	}

	void updateGapBtn(float) {
		auto* f = m_fields.self();
		if (!f->btn) return;

		auto* menu = m_copyBtn ? typeinfo_cast<CCMenu*>(m_copyBtn->getParent()) : nullptr;
		if (!menu || !menu->getChildren()) {
			f->btn->setVisible(false);
			return;
		}

		// Find the bottom row of the grid; the color picker is its first cell
		std::vector<CCPoint> positions;
		float minY = 1e9f;
		for (auto child : CCArrayExt<CCNode*>(menu->getChildren())) {
			if (child == f->btn.data()) continue;
			if (!typeinfo_cast<CCMenuItem*>(child)) continue;
			positions.push_back(child->getPosition());
			minY = std::min(minY, child->getPositionY());
		}
		std::vector<float> row;
		for (auto const& p : positions) {
			if (std::fabs(p.y - minY) < 2.f) row.push_back(p.x);
		}
		if (positions.size() < 4 || row.size() < 2) {
			f->btn->setVisible(false);
			return;
		}
		std::sort(row.begin(), row.end());
		float cell = row[1] - row[0];
		if (cell < 1.f) {
			f->btn->setVisible(false);
			return;
		}

		if (f->btn->getParent() != menu) {
			f->btn->removeFromParent();
			menu->addChild(f->btn, 10);
		}

		auto* mod = Mod::get();
		float offX = static_cast<float>(mod->getSettingValue<double>("button-offset-x"));
		float offY = static_cast<float>(mod->getSettingValue<double>("button-offset-y"));
		f->btn->setPosition({ row[0] - cell + offX, minY + offY });

		float w = f->btn->getContentSize().width;
		if (w > 1.f) {
			float s = std::clamp(cell * 0.85f / w, 0.2f, 2.f);
			if (std::fabs(s - f->lastScale) > 0.001f) {
				f->btn->setScale(s);
				f->lastScale = s;
			}
		}
		f->btn->setVisible(true);
	}

	void onGapCalc(CCObject*) {
		auto objs = this->getSelectedObjects();
		GameObject* a = nullptr;
		GameObject* b = nullptr;
		if (objs && objs->count() == 2) {
			a = typeinfo_cast<GameObject*>(objs->objectAtIndex(0));
			b = typeinfo_cast<GameObject*>(objs->objectAtIndex(1));
		}
		if (auto popup = GapPopup::create(this, a, b)) popup->show();
	}
};
