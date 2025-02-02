#include "config.hpp"

using namespace geode::prelude;


struct SelInfo : CCObject {
	SEL_MenuHandler m_defaultSelector;
	SelInfo(SEL_MenuHandler defaultSelector) : m_defaultSelector(defaultSelector) {
		this->autorelease();
	}
};


class $modify(MyEditorUI, EditorUI) {
	struct Fields {
		// settings
		short m_totalButtons = 20;

		// editor things
		bool m_reloadRequired = false;

		Ref<EditButtonBar> m_myBar;
		Ref<EditButtonBar> m_otherBar;
		Ref<CreateMenuItem> m_activeBtn;
		Ref<CreateMenuItem> m_deleteBtn;

		// set and list with recent objects
		std::set<short> m_rSet;
		std::list<short> m_rList;

		Fields () {
			// get settings
			m_totalButtons = Mod::get()->getSettingValue<int64_t>("max-count");

			// load data
			auto saveStr = Mod::get()->getSavedValue<std::string>("recent");
			std::istringstream ss(saveStr);
			std::string token;
			for (int i = 0; std::getline(ss, token, ',') && i < m_totalButtons;) {
				short id = std::atoi(token.c_str());
				if(id > 0 && m_rSet.insert(id).second) {
					m_rList.push_back(id);
					i++;
				}
			}
			log::debug("Loaded {}/{} items", m_rSet.size(), m_totalButtons);
		};

		~Fields () {
			// save data
			std::string save;
			for (short id : m_rList) {
				save += std::to_string(id) + ",";
			}
			Mod::get()->setSavedValue<std::string>("recent", save);
			log::debug("Saved {} items", m_rSet.size());
		};
	};

	// custom selector for create buttons in my tab
	void onItemClick(CCObject* sender) {
		auto cmi = static_cast<CreateMenuItem*>(sender);
		auto selInfo = static_cast<SelInfo*>(cmi->getUserObject());
		auto method = static_cast< void(CCObject::*)(CCObject*) >(selInfo->m_defaultSelector); // kill me

		(this->*method)(sender); // call original selector

		if (this->m_selectedObjectIndex == cmi->m_objectID) {
			toggleButton(cmi, true);
			m_fields->m_activeBtn = cmi;
		} else {
			toggleButton(cmi, false);
			m_fields->m_activeBtn = nullptr;
		}
	}

	// selector for delete button in my tab
	void onDeleteClick(CCObject*) {
		if (!m_fields->m_activeBtn) return;

		handleNewObject(m_fields->m_activeBtn->m_objectID, true);

		// remove from tab
		m_fields->m_myBar->m_buttonArray->removeObject(m_fields->m_activeBtn);

		reloadBar(false); // to show changes
		onItemClick(m_fields->m_activeBtn); // deselect button

		// unregister button
		for (int iter = this->m_createButtonArray->count() - 1; iter >= 0; iter--) {
			if (m_fields->m_activeBtn == this->m_createButtonArray->objectAtIndex(iter)) {
				this->m_createButtonArray->removeObjectAtIndex(iter);
				break;
			}
		}
		m_fields->m_activeBtn = nullptr;
	}

	// void onDebugButton(CCObject*) {
	// 	log::debug("registered: {}; size_1: {}; size_2: {}", 
	// 		this->m_createButtonArray->count(), m_fields->m_rList.size(), m_fields->m_rSet.size());
	// }

	// get create button with the right color and my custom selector
	CreateMenuItem* advancedGetCreateBtn(int id) {
		CreateMenuItem* cmi;
		if (darkerButtonBgObjIds.contains(id)) cmi = getCreateBtn(id, 5); // darker color
		else cmi = getCreateBtn(id, 4); // lighter color

		cmi->setUserObject(new SelInfo(cmi->m_pfnSelector));
		cmi->m_pfnSelector = menu_selector(MyEditorUI::onItemClick); 
		return cmi;
	}

	CreateMenuItem* getDeleteButton() {
		auto cmi = getCreateBtn(83, 6);
		if (this->m_createButtonArray->lastObject() == cmi) {
			this->m_createButtonArray->removeLastObject();
		}
		auto btnSpr = static_cast<ButtonSprite*>(cmi->getChildren()->objectAtIndex(0));
		auto children = btnSpr->getChildren();
		for (unsigned i = 0; i < children->count(); i++) {
			if (auto child = typeinfo_cast<GameObject*>(children->objectAtIndex(i))) {
				auto spr = CCSprite::createWithSpriteFrameName("edit_delCBtn_001.png");
				btnSpr->addChild(spr, child->getZOrder());
				spr->setPosition(child->getPosition());
				// child->setOpacity(0);
				child->removeFromParent();
				break;
			}
		}
		cmi->m_pfnSelector = menu_selector(MyEditorUI::onDeleteClick); 
		cmi->m_objectID = 0;
		return cmi;
	}

	$override
	bool init(LevelEditorLayer* editorLayer) {
		if (!EditorUI::init(editorLayer)) return false;

		m_fields->m_otherBar = static_cast<EditButtonBar*>(this->m_createButtonBars->objectAtIndex(0));
		m_fields->m_deleteBtn = getDeleteButton();

		EditorTabs::get()->addTab(this, TabType::BUILD, "recent-objects-tab"_spr, 
			create_tab_callback(MyEditorUI::loadMyTab),
			toggle_tab_callback(MyEditorUI::toggleMyTab));

		// debug button
		// auto myButton = CCMenuItemSpriteExtra::create(
		// 	CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png"),
		// 	this, menu_selector(MyEditorUI::onDebugButton)
		// );
		// auto menu = this->getChildByID("undo-menu");
		// menu->addChild(myButton);
		// menu->updateLayout();

		return true;
	}

	// on first load
	CCNode* loadMyTab(EditorUI* ui, CCMenuItemToggler* toggler) {
		// create icon for build tab
		auto icon = CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png");
        EditorTabUtils::setTabIcon(toggler, icon);
		
		// init the tab with empty array
		auto cols = GameManager::sharedState()->getIntGameVariable("0049");
		auto rows = GameManager::sharedState()->getIntGameVariable("0050");

		m_fields->m_myBar = EditButtonBar::create(CCArray::create(), m_fields->m_otherBar->m_position, 30, true, cols, rows);
		m_fields->m_reloadRequired = true;

        return m_fields->m_myBar;
    }

	// this is automatically called on clicking on any build tab
	void toggleMyTab(EditorUI* ui, bool state, cocos2d::CCNode* bar) {
		static bool isOpen = false;
		if (!state) { // means other tab was opened
			isOpen = false;
			return;
		};
		if (isOpen && m_fields->m_myBar->m_buttonArray->count()) return; // means my tab was already opened
		isOpen = true;

		if (m_fields->m_reloadRequired || !m_fields->m_myBar->m_buttonArray->count()) {
			reloadBar();
			m_fields->m_reloadRequired = false;
			updateCreateMenu(false); // false - no jump to another page
		}
		activateButtonOnMyBar();
    }

	// take ids from m_fields->m_rList and create a bar with buttons of objects with these ids
	void reloadBar(bool fullReload=true) {
		if (fullReload) {
			// unregister old buttons
			int iter = this->m_createButtonArray->count() - 1;
			for (int i = m_fields->m_myBar->m_buttonArray->count() - 2; i >= 0; i--) {
				for (;iter >= 0; iter--) {
					if (m_fields->m_myBar->m_buttonArray->objectAtIndex(i) == this->m_createButtonArray->objectAtIndex(iter)) {
						this->m_createButtonArray->removeObjectAtIndex(iter);
						break;
					}
				}
			}
			// clear old buttons
			m_fields->m_myBar->m_buttonArray->removeAllObjects();
			
			// add new buttons
			for (auto id : m_fields->m_rList) {
				auto btn = advancedGetCreateBtn(id);
				m_fields->m_myBar->m_buttonArray->addObject(btn);
			}
			m_fields->m_myBar->m_buttonArray->addObject(m_fields->m_deleteBtn); // add delete button
		}
		// if (m_fields->m_myBar->m_buttonArray->count()) {
		// 	auto first = static_cast<CCNode*>(m_fields->m_myBar->m_buttonArray->objectAtIndex(0));
		// 	if (auto btnSpr = typeinfo_cast<ButtonSprite*>(first->getChildren()->objectAtIndex(0))) {
		// 		if (auto btnChildren = btnSpr->getChildren()) {
		// 			for (unsigned i = 0; i < btnChildren->count(); i++) {
		// 				if (auto gameObj = typeinfo_cast<GameObject*>(btnChildren->objectAtIndex(i))) {
		// 					// prevent Object Groups (yeah, my other mod...) from adding groups to the tab
		// 					gameObj->m_objectID = 83; 
		// 					break;
		// 				}
		// 			}
		// 		}
		// 	}
		// }
		auto cols = GameManager::sharedState()->getIntGameVariable("0049");
		auto rows = GameManager::sharedState()->getIntGameVariable("0050");
		m_fields->m_myBar->loadFromItems(m_fields->m_myBar->m_buttonArray, cols, rows, true); // reload
	}

	// activated button is darker then others
	void toggleButton(CreateMenuItem* cmi, bool activate) {
		float ratio = activate ? .5f : 2.f;
		auto btnSpr = typeinfo_cast<ButtonSprite*>(cmi->getChildren()->objectAtIndex(0));
		if (!btnSpr) return; 
		if (auto ch = btnSpr->getChildren()) recursiveSetChildrenColor(ratio, ch);
	}

	// this is called only from toggleButton() function 
	void recursiveSetChildrenColor(float ratio, CCArray* array) {
		for (unsigned i = 0; i < array->count(); i++) {
			auto child = typeinfo_cast<CCSprite*>(array->objectAtIndex(i));
			if (!child) continue;
			auto c = child->getColor();
			if (ratio < 1) child->setColor(ccc3(
				(uint8_t) (c.r < 128 ? c.r : c.r * ratio), 
				(uint8_t) (c.g < 128 ? c.g : c.g * ratio), 
				(uint8_t) (c.b < 128 ? c.b : c.b * ratio)
			));
			else child->setColor(ccc3(
				(uint8_t) (c.r < 128 ? c.r * ratio : c.r), 
				(uint8_t) (c.g < 128 ? c.g * ratio : c.g), 
				(uint8_t) (c.b < 128 ? c.b * ratio : c.b)
			));
			if (auto ch = child->getChildren()) recursiveSetChildrenColor(ratio, ch);
		}
	}

	// make button on my bar activated if its object is selected in another tab now
	void activateButtonOnMyBar() {
		m_fields->m_activeBtn = nullptr;
		for (unsigned i = 0; i < m_fields->m_myBar->m_buttonArray->count() - 1; i++) {
			auto cmi = static_cast<CreateMenuItem*>(m_fields->m_myBar->m_buttonArray->objectAtIndex(i));
			if (cmi->m_objectID == this->m_selectedObjectIndex) {
				toggleButton(cmi, true);
				m_fields->m_activeBtn = cmi;
				break;
			}
		}
	}

	$override
	bool onCreate() {
		bool ret = EditorUI::onCreate();
		short objId = this->m_selectedObjectIndex;
		if (objId > 0) {
			handleNewObject(objId);
			m_fields->m_reloadRequired = true;
		}
		return ret;
	}

	void handleNewObject(short objId, bool isDelete=false) {
		if (m_fields->m_rSet.contains(objId)) {
			// move object to the front of the list
			auto it = std::find(m_fields->m_rList.begin(), m_fields->m_rList.end(), objId);
			if (it != m_fields->m_rList.end()) {
				m_fields->m_rList.erase(it);
				if (isDelete) {
					m_fields->m_rSet.erase(objId);
				} else {
					m_fields->m_rList.push_front(objId);
				}
			}
		} else {
			if (m_fields->m_rList.size() >= m_fields->m_totalButtons) {
				m_fields->m_rSet.erase(m_fields->m_rList.back());
				m_fields->m_rList.pop_back();
			}
			m_fields->m_rSet.insert(objId);
			m_fields->m_rList.push_front(objId);
		}
	}
};