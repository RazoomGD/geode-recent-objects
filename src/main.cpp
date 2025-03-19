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
		bool m_isFreezeMode = false;
		
		Ref<EditButtonBar> m_myBar;
		Ref<EditButtonBar> m_otherBar;
		Ref<CreateMenuItem> m_activeBtn;
		Ref<CreateMenuItem> m_deleteBtn;
		Ref<CreateMenuItem> m_freezeBtn;
		
		// set and list with recent objects
		std::set<short> m_rSet;
		std::list<short> m_rList;

		Fields () {
			// get settings
			m_totalButtons = Mod::get()->getSettingValue<int64_t>("max-count");
			
			m_isFreezeMode = Mod::get()->getSavedValue<bool>("is-freezed", false);
			
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
			// log::debug("Loaded {}/{} items", m_rSet.size(), m_totalButtons);
		};

		~Fields () {
			// save data
			std::string save;
			for (short id : m_rList) {
				save += std::to_string(id) + ",";
			}
			Mod::get()->setSavedValue<std::string>("recent", save);
			Mod::get()->setSavedValue<bool>("is-freezed", m_isFreezeMode);
			// log::debug("Saved {} items", m_rSet.size());
		};
	};

	// custom selector for create buttons in my tab
	void onItemClick(CCObject* sender) {
		auto cmi = static_cast<CreateMenuItem*>(sender);
		auto selInfo = static_cast<SelInfo*>(cmi->getUserObject("selInfo"_spr));

		(cmi->m_pListener->*(selInfo->m_defaultSelector))(sender); // call original selector

		if (this->m_selectedObjectIndex == cmi->m_objectID) {
			setColorToCreateButton(cmi, false);
			m_fields->m_activeBtn = cmi;
		} else {
			setColorToCreateButton(cmi, true);
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
		unregisterButton(m_fields->m_activeBtn);
		m_fields->m_activeBtn = nullptr;
	}

	// selector for freeze button in my tab
	void onFreezeClick(CCObject* btn) {
		m_fields->m_isFreezeMode = !m_fields->m_isFreezeMode;
		setColorToCreateButton(static_cast<CreateMenuItem*>(btn), !m_fields->m_isFreezeMode);
	}

	// get create button with the right color and my custom selector
	CreateMenuItem* advancedGetCreateBtn(int id) {
		CreateMenuItem* cmi;
		if (darkerButtonBgObjIds.contains(id)) cmi = getCreateBtn(id, 5); // darker color
		else cmi = getCreateBtn(id, 4); // lighter color

		cmi->setUserObject("selInfo"_spr, new SelInfo(cmi->m_pfnSelector));
		cmi->m_pfnSelector = menu_selector(MyEditorUI::onItemClick); 
		return cmi;
	}

	CreateMenuItem* getDeleteButton() {
		auto btnSpr = ButtonSprite::create(CCSprite::createWithSpriteFrameName("edit_delCBtn_001.png"), 32, 0, 32, 1, true, "GJ_button_06.png", true);
		auto cmi = CreateMenuItem::create(btnSpr, nullptr, this, menu_selector(MyEditorUI::onDeleteClick));
		cmi->m_objectID = 0;
		return cmi;
	}

	CreateMenuItem* getFreezeButton() {
		auto btnSpr = ButtonSprite::create(CCSprite::create("snowflake.png"_spr), 32, 0, 32, 1, true, "GJ_button_02.png", true);
		auto cmi = CreateMenuItem::create(btnSpr, nullptr, this, menu_selector(MyEditorUI::onFreezeClick));
		cmi->m_objectID = 0;
		if (m_fields->m_isFreezeMode) {
			setColorToCreateButton(cmi, false);
		}
		return cmi;
	}

	$override
	bool init(LevelEditorLayer* editorLayer) {
		if (!EditorUI::init(editorLayer)) return false;

		m_fields->m_otherBar = static_cast<EditButtonBar*>(this->m_createButtonBars->objectAtIndex(0));
		m_fields->m_deleteBtn = getDeleteButton();
		if (Mod::get()->getSettingValue<bool>("show-freeze-btn")) {
			m_fields->m_freezeBtn = getFreezeButton();
		} else {
			m_fields->m_isFreezeMode = false;
		}

		EditorTabs::get()->addTab(this, TabType::BUILD, "recent-objects-tab"_spr, 
			create_tab_callback(MyEditorUI::loadMyTab),
			toggle_tab_callback(MyEditorUI::toggleMyTab));

		return true;
	}

	// on first load
	CCNode* loadMyTab(EditorUI* ui, CCMenuItemToggler* toggler) {
        EditorTabUtils::setTabIcon(toggler, CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png"));
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
			for (auto* btn : CCArrayExt<CreateMenuItem*>(m_fields->m_myBar->m_buttonArray)) {
				if (btn->m_objectID == 0) break;
				unregisterButton(btn);
			}
			// clear old buttons
			m_fields->m_myBar->m_buttonArray->removeAllObjects();
			
			// add new buttons
			for (auto id : m_fields->m_rList) {
				auto btn = advancedGetCreateBtn(id);
				m_fields->m_myBar->m_buttonArray->addObject(btn);
			}
			m_fields->m_myBar->m_buttonArray->addObject(m_fields->m_deleteBtn); // add delete button
			if (m_fields->m_freezeBtn) {
				m_fields->m_myBar->m_buttonArray->addObject(m_fields->m_freezeBtn); // add freeze button
			}
		}

		auto cols = GameManager::sharedState()->getIntGameVariable("0049");
		auto rows = GameManager::sharedState()->getIntGameVariable("0050");
		m_fields->m_myBar->loadFromItems(m_fields->m_myBar->m_buttonArray, cols, rows, true); // reload
	}


	void setColorToCreateButton(CreateMenuItem* cmi, bool isBright) {
		// ! mostly decompiled code of EditorUI::updateCreateMenu() that sets the color

		ccColor3B color = isBright ? ccc3(255, 255, 255) : ccc3(127, 127, 127);
		if (auto spr = cmi->getChildByType<ButtonSprite>(0)) {
			if (spr->m_subBGSprite) {
				spr->m_subBGSprite->setColor(color); // button bg sprite
			}
			if (auto gameObj = typeinfo_cast<GameObject*>(spr->m_subSprite)) {
				int objId;
				if (gameObj->m_classType == 1) {
					bool cVar14;
					if (gameObj->m_customColorType == 0) {
						cVar14 = gameObj->m_maybeNotColorable;
					} else {
						cVar14 = (gameObj->m_customColorType == 1);
					}
					if (cVar14 || gameObj->m_colorSprite || 
						gameObj->m_baseColor->m_defaultColorID == 0x3ec || 
						gameObj->m_baseColor->m_defaultColorID == 0x0  || 
						/* (*(char *)(gameObj + 0xdf) == '\0') */ false) {
						goto LAB_14010da56;
					}
					color = isBright ? ccc3(200, 200, 255) : ccc3(100, 100, 127);
					goto LAB_14010dac2;
				}
	LAB_14010da56:
				objId = gameObj->m_objectID;
				bool bVar12;
				if (objId < 0x531) {
					if (((objId == 0x530) || (objId == 0x396)) || (objId == 0x397)) goto LAB_14010daad;
					bVar12 = (objId == 0x52f);
	LAB_14010da89:
					if (bVar12) goto LAB_14010daad;
					auto piVar3 = gameObj->m_baseColor;
					if (piVar3 != 0) {
						objId = piVar3->m_colorID;
						if ((piVar3->m_defaultColorID == objId) || (objId == 0x0)) {
							objId = piVar3->m_defaultColorID;
						}
						if (objId == 0x3f2) goto LAB_14010daad;
					}
				}
				else {
					if (objId != 0x630) {
						bVar12 = (objId == 0x7dc);
						goto LAB_14010da89;
					}
	LAB_14010daad:
					color = isBright ? ccc3(0, 0, 0) : ccc3(127, 127, 127);
				}
	LAB_14010dac2:
				gameObj->setObjectColor(color);
				color = isBright ? ccc3(200, 200, 255) : ccc3(100, 100, 127);
				gameObj->setChildColor(color);

			} else if (spr->m_subSprite) {
				spr->m_subSprite->setColor(color);
			}
		}
	}

	// make button on my bar activated if its object is selected in another tab now
	void activateButtonOnMyBar() {
		m_fields->m_activeBtn = nullptr;
		for (unsigned i = 0; i < m_fields->m_myBar->m_buttonArray->count(); i++) {
			auto cmi = static_cast<CreateMenuItem*>(m_fields->m_myBar->m_buttonArray->objectAtIndex(i));
			if (cmi->m_objectID == 0) break; // reached delete button
			if (cmi->m_objectID == m_selectedObjectIndex) {
				setColorToCreateButton(cmi, false);
				m_fields->m_activeBtn = cmi;
				break;
			}
		}
	}

	void unregisterButton(CreateMenuItem* cmi) {
		for (int iter = m_createButtonArray->count() - 1; iter >= 0; iter--) {
			if (cmi == m_createButtonArray->objectAtIndex(iter)) {
				m_createButtonArray->removeObjectAtIndex(iter);
				break;
			}
		}
	}

	$override
	bool onCreate() {
		bool ret = EditorUI::onCreate();
		if (!m_fields->m_isFreezeMode) {
			short objId = m_selectedObjectIndex;
			if (objId > 0) {
				handleNewObject(objId);
				m_fields->m_reloadRequired = true;
			}
		}
		return ret;
	}

	void handleNewObject(short objId, bool isDelete=false) {
		if (m_fields->m_rSet.contains(objId)) {
			// move object to the front of the list
			auto it = std::find(m_fields->m_rList.begin(), m_fields->m_rList.end(), objId);
			if (it != m_fields->m_rList.end()) {
				if (isDelete) {
					m_fields->m_rList.erase(it);
					m_fields->m_rSet.erase(objId);
				} else {
					m_fields->m_rList.erase(it);
					m_fields->m_rList.push_front(objId);
				}
			}
		} else {
			if (!isDelete) {
				if (m_fields->m_rList.size() >= m_fields->m_totalButtons) {
					m_fields->m_rSet.erase(m_fields->m_rList.back());
					m_fields->m_rList.pop_back();
				}
				m_fields->m_rSet.insert(objId);
				m_fields->m_rList.push_front(objId);
			}
		}
	}
};