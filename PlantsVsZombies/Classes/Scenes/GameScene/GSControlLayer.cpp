/**
 *Copyright (c) 2020 LZ.All Right Reserved
 *Author : LZ
 *Date: 2020.1.28
 *Email: 2117610943@qq.com
 */

#include "GSControlLayer.h"
#include "GSDefine.h"
#include "GSData.h"
#include "GSGameResultJudgement.h"
#include "GSGameEndLayer.h"
#include "GSButtonLayer.h"
#include "GSInformationLayer.h"
#include "GSBackgroundLayer.h"
#include "GSAnimationLayer.h"

#include "Plants/Plants.h"
#include "Plants/Plants-Files.h"
#include "Zombies/Zombies.h"
#include "Based/LevelData.h"
#include "Based/GameType.h"
#include "Scenes/EasterEggsScene/GameEasterEggs.h"

GameMapInformation::GameMapInformation():
   rowNumbers(5)
,  columnNumbers(9)
{
	MAP_INIT(plantsMap);
	MAP_CAN_NOT_PLANT(plantsMap);
}

GSControlLayer::GSControlLayer():
	_global(Global::getInstance())
,   _openLevelData(OpenLevelData::getInstance())
,	_gameMapInformation(nullptr)
,   _selectPlantsTag(PlantsType::None)
,   _plantPreviewImage(nullptr)
,   _animationLayer(nullptr)
,   _gameEndShieldLayer(nullptr)
,   _zombiesAppearControl(nullptr)
{
	srand(time(nullptr));
	_levelData = _openLevelData->readLevelData(_openLevelData->getLevelNumber())->getMunchZombiesFrequency();
	_gameMapInformation = new GameMapInformation();
	_zombiesAppearControl = new ZombiesAppearControl();
}

GSControlLayer::~GSControlLayer()
{
	delete _gameMapInformation;
	delete _zombiesAppearControl;
}

bool GSControlLayer::init()
{
	if(!Layer::init())return false;

	createSchedule();
	createZombiesHowlSoundEffect();
	createPlantsCardListener();
	createMouseListener();

	return true;
}

void GSControlLayer::createSchedule()
{
	schedule([&](float){
			controlCardEnabled();
			calculatePlantPosition();
			createZombies();
			controlRefurbishMusicAndText();
			judgeLevelIsFinished();
		}, 0.1f, "mainUpdate");

	schedule([&](float) {
		zombiesComeTiming();
		}, 1.0f, "zombiesComing");
}

void GSControlLayer::createZombiesHowlSoundEffect()
{
	this->runAction(RepeatForever::create(Sequence::create(DelayTime::create(1.0f),
		CallFunc::create([=]()
			{
				if (rand() % 10000 <= 700)
				{
					const string music[6] = { "groan","groan2","groan3","groan4","groan5","groan6" };
					AudioEngine::setVolume(AudioEngine::play2d(_global->userInformation->getMusicPath().find(music[rand() % 6])->second), _global->userInformation->getSoundEffectVolume());
				}
			}), nullptr)));
}

void GSControlLayer::controlCardEnabled()
{
	for (auto& card : _global->userInformation->getUserSelectCrads())
	{
		/* 如果植物所需阳光数大于拥有阳光数 */
		if (buttonLayerInformation->plantsCards[card.cardTag].plantsNeedSunNumbers > _global->userInformation->getSunNumbers())
		{
			buttonLayerInformation->plantsCards[card.cardTag].plantsCardText->setColor(Color3B::RED);
		}
		else
		{
			buttonLayerInformation->plantsCards[card.cardTag].plantsCardText->setColor(Color3B::BLACK);
		}
		/* 如果卡牌填充倒计时完成 */
		if (buttonLayerInformation->plantsCards[card.cardTag].timeBarIsFinished)
		{
			buttonLayerInformation->plantsCards[card.cardTag].plantsCards->setEnabled(true);
			/* 如果植物所需阳光数小于阳光数 */
			if (buttonLayerInformation->plantsCards[card.cardTag].plantsNeedSunNumbers <= _global->userInformation->getSunNumbers())
			{
				buttonLayerInformation->plantsCards[card.cardTag].plantsCards->setColor(Color3B::WHITE);
			}
			else
			{
				buttonLayerInformation->plantsCards[card.cardTag].plantsCards->setColor(Color3B::GRAY);
			}
		}
	}
}

void GSControlLayer::calculatePlantPosition()
{
	for (int i = 0; i < _gameMapInformation->rowNumbers; i++)
	{
		for (int j = 0; j < _gameMapInformation->columnNumbers; j++)
		{
			if (GRASS_INSIDE(_cur, i, j))
			{
				_plantsPosition.x = j;
				_plantsPosition.y = i;
			}
		}
	}

	/* 如果不在范围内，移除到画面外 */
	if (GRASS_OUTSIDE(_cur))
	{
		_plantsPosition.x = -100;
		_plantsPosition.y = -100;
	}
}

void GSControlLayer::createMouseListener()
{
	/* 创建鼠标监听 */
	auto Mouse = EventListenerMouse::create();

	/* 鼠标移动 */
	Mouse->onMouseMove = [&](Event* event)
	{
		/* 获取鼠标位置 */
		_cur = ((EventMouse*)event)->getLocationInView();

		mouseMoveControl();
	};

	/* 鼠标按下 */
	Mouse->onMouseDown = [&](Event* event)
	{
		mouseDownControl((EventMouse*)event);
	};

	_director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(Mouse, this);
}

void GSControlLayer::createPlantsCardListener()
{
	for (auto& card : _global->userInformation->getUserSelectCrads())
	{
		buttonLayerInformation->plantsCards[card.cardTag].plantsCards->addTouchEventListener([&, card](Ref* sender, ui::Widget::TouchEventType type)
			{
				switch (type)
				{
				case ui::Widget::TouchEventType::ENDED:
					if (!buttonLayerInformation->mouseSelectImage->isSelectPlants)
					{
						_selectPlantsTag = static_cast<PlantsType>(card.cardTag);
					}
					selectPlantsPreviewImage();
					break;
				}
			});
	}
}

void GSControlLayer::selectPlantsPreviewImage()
{
	switch (buttonLayerInformation->mouseSelectImage->isSelectPlants)
	{
	case true:
		AudioEngine::setVolume(AudioEngine::play2d(_global->userInformation->getMusicPath().find("tap2")->second), _global->userInformation->getSoundEffectVolume());
		buttonLayerInformation->plantsCards[static_cast<int>(_selectPlantsTag)].progressTimer->setPercentage(0);
		buttonLayerInformation->plantsCards[static_cast<int>(_selectPlantsTag)].plantsCards->setColor(Color3B::WHITE);

		/* 加上所需的阳光数并更新 */
		_global->userInformation->setSunNumbers(_global->userInformation->getSunNumbers() + buttonLayerInformation->plantsCards[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)].plantsNeedSunNumbers);
		informationLayerInformation->updateSunNumbers();

		removePreviewPlant();

		/* 植物要求更新 */
		backgroundLayerInformation->gameType->updateRequirementNumbers("植物数量增加");
		break;
	case false:
		/* 如果植物所需阳光大于拥有阳光，则发出警告 */
		if (buttonLayerInformation->plantsCards[static_cast<int>(_selectPlantsTag)].plantsNeedSunNumbers > _global->userInformation->getSunNumbers())
		{
			AudioEngine::setVolume(AudioEngine::play2d(_global->userInformation->getMusicPath().find("buzzer")->second), _global->userInformation->getSoundEffectVolume());
			informationLayerInformation->sunNumberTextWarning();
		}
		/* 剩余植物数量小于等于0 */
		else if (backgroundLayerInformation->gameType->getPlantsRequriement()->isHavePlantsRequriement && backgroundLayerInformation->gameType->getPlantsRequriement()->surPlusPlantsNumbers <= 0)
		{
			AudioEngine::setVolume(AudioEngine::play2d(_global->userInformation->getMusicPath().find("buzzer")->second), _global->userInformation->getSoundEffectVolume());
			backgroundLayerInformation->gameType->waringPlantsNull();
		}
		else
		{
			AudioEngine::setVolume(AudioEngine::play2d(_global->userInformation->getMusicPath().find("seedlift")->second), _global->userInformation->getSoundEffectVolume());

			/* 减去所需的阳光数并更新 */
			_global->userInformation->setSunNumbers(_global->userInformation->getSunNumbers() - buttonLayerInformation->plantsCards[static_cast<int>(_selectPlantsTag)].plantsNeedSunNumbers);
			informationLayerInformation->updateSunNumbers();

			/* 卡片变黑 */
			buttonLayerInformation->plantsCards[static_cast<int>(_selectPlantsTag)].plantsCards->setColor(Color3B::GRAY);
			buttonLayerInformation->plantsCards[static_cast<int>(_selectPlantsTag)].progressTimer->setPercentage(100);

			/* 鼠标选择标记 */
			buttonLayerInformation->mouseSelectImage->isSelectPlants = true;
			buttonLayerInformation->mouseSelectImage->selectPlantsId = _selectPlantsTag;

			createPreviewPlants();

			/* 植物要求更新 */
			backgroundLayerInformation->gameType->updateRequirementNumbers("植物数量减少");
		}
		break;
	}
}

void GSControlLayer::createPreviewPlants()
{
	CURSOR_VISIBLE(false)

	Plants* previewPlants, * curPlants;/* 预览植物 */
	switch (_selectPlantsTag)
	{
	case PlantsType::SunFlower:          previewPlants = SunFlower::create(this);         curPlants = SunFlower::create(this);         break;
	case PlantsType::PeaShooter:         previewPlants = PeaShooter::create(this);        curPlants = PeaShooter::create(this);        break;
	case PlantsType::WallNut:            previewPlants = WallNut::create(this);           curPlants = WallNut::create(this);           break;
	case PlantsType::CherryBomb:         previewPlants = CherryBomb::create(this);        curPlants = CherryBomb::create(this);        break;
	case PlantsType::PotatoMine:         previewPlants = PotatoMine::create(this);        curPlants = PotatoMine::create(this);        break;
	case PlantsType::CabbagePult:        previewPlants = CabbagePult::create(this);       curPlants = CabbagePult::create(this);       break;
	case PlantsType::Torchwood:          previewPlants = Torchwood::create(this);         curPlants = Torchwood::create(this);         break;
	case PlantsType::Spikeweed:          previewPlants = Spikeweed::create(this);         curPlants = Spikeweed::create(this);         break;
	case PlantsType::Garlic:             previewPlants = Garlic::create(this);            curPlants = Garlic::create(this);            break;
	case PlantsType::FirePeaShooter:     previewPlants = FirePeaShooter::create(this);    curPlants = FirePeaShooter::create(this);    break;
	case PlantsType::Jalapeno:           previewPlants = Jalapeno::create(this);          curPlants = Jalapeno::create(this);          break;
	case PlantsType::AcidLemonShooter:   previewPlants = AcidLemonShooter::create(this);  curPlants = AcidLemonShooter::create(this);  break;
	case PlantsType::Citron:             previewPlants = Citron::create(this);            curPlants = Citron::create(this);            break;
	default: break;
	}
	
	_plantPreviewImage = previewPlants->createPlantImage();

	_plantCurImage = curPlants->createPlantImage();
	_plantCurImage->setOpacity(255);
	_plantCurImage->setPosition(_cur);
	_plantCurImage->setGlobalZOrder(10);
}

void GSControlLayer::zombiesComeTiming()
{
	if (!_zombiesAppearControl->getIsBegin())
	{
		_zombiesAppearControl->setTimeClear();
		_zombiesAppearControl->setIsBegin(true);
	}

	/* 计时 */
	_zombiesAppearControl->setTimeAdd();
}

void GSControlLayer::createZombies()
{
	/* 刷新僵尸 */
	if (_zombiesAppearControl->getLastFrequencyZombiesWasDeath())
	{
		_zombiesAppearControl->setLastFrequencyZombiesWasDeath(false);
		if (_zombiesAppearControl->getZombiesAppearFrequency() < _openLevelData->readLevelData(_openLevelData->getLevelNumber())->getZombiesFrequency())
		{
			int zombiesNumbers = _zombiesAppearControl->getZombiesNumbersForAppearFrequency(_zombiesAppearControl->getZombiesAppearFrequency());
			for (int i = 0; i < zombiesNumbers; ++i)
			{
				animationLayerInformation->createZombies();
			}
			/* 进攻波数自增一 */
			_zombiesAppearControl->setZombiesAppearFrequency();

			/* 进度条更新 */
			informationLayerInformation->updateProgressBar(_zombiesAppearControl->getZombiesAppearFrequency());
		}
	}
	informationLayerInformation->updateProgressBarHead();
	
	/* 控制僵尸的刷新 */
	if (controlRefurbishZombies())
	{
		if (_zombiesAppearControl->getZombiesAppearFrequency())
		{
			_zombiesAppearControl->setTimeClear(); /* 距离上一波刷新时间清零 */
		}
		_zombiesAppearControl->setLastFrequencyZombiesWasDeath(true);
		_zombiesAppearControl->setIsBegin(false);
		UserDefault::getInstance()->flush();
	}
}

bool GSControlLayer::controlRefurbishZombies()
{
	if ((_zombiesAppearControl->getTime() >= _openLevelData->readLevelData(_openLevelData->getLevelNumber())->getFirstFrequencyTime()&&
		_zombiesAppearControl->getZombiesAppearFrequency() == 0)                   /* 第一波刷新控制 */

		|| (_zombiesAppearControl->getTime() >= 32 + rand() % 10 &&
		   (_zombiesAppearControl->getZombiesAppearFrequency() == 1 
	    || _zombiesAppearControl->getZombiesAppearFrequency() == 2))               /* 第二三波刷新控制 */

		|| _zombiesAppearControl->getTime() >= 45                                  /* 如果大于45秒刷新下一波 */

		|| (Zombies::getZombiesNumbers() <= 4 &&
			_zombiesAppearControl->getZombiesAppearFrequency() > 3)                /* 如果活着的僵尸数小于规定，刷新下一波 */

		|| (Zombies::getZombiesNumbers() <= 0 &&                                   /* 如果没有存活僵尸则立即刷新僵尸 */
			_zombiesAppearControl->getZombiesAppearFrequency() != 0)
	   )
	{
		return true;
	}
	return false;
}

void GSControlLayer::controlRefurbishMusicAndText()
{
	/* 控制大波僵尸来袭的文字与音乐 */
	auto level = _openLevelData->readLevelData(_openLevelData->getLevelNumber());
	if (_zombiesAppearControl->getTime() >= level->getFirstFrequencyTime() && _zombiesAppearControl->getZombiesAppearFrequency() == 0)
	{
		AudioEngine::setVolume(AudioEngine::play2d(_global->userInformation->getMusicPath().find("awooga")->second), _global->userInformation->getSoundEffectVolume());
	}

	/* 最后一波僵尸的文字显示与旗子更新 */
	if (_zombiesAppearControl->getZombiesAppearFrequency() == level->getZombiesFrequency() && !_zombiesAppearControl->getIsShowWords())
	{
		if (informationLayerInformation->updateProgressBarFlag())
		{
			_zombiesAppearControl->setIsShowWords(true);
		}
	}

	/* 前几波文字显示与旗子更新 */
	for (auto data = _levelData.begin(); data != _levelData.end();)
	{
		if (_zombiesAppearControl->getZombiesAppearFrequency() == *data)
		{
			if (informationLayerInformation->updateProgressBarFlag(-1) && informationLayerInformation->updateProgressBarFlag(*data))
			{
				data = _levelData.erase(data);
			}
			else
			{
				++data;
			}
		}
		else 
		{
			++data;
		}
	}
}

bool GSControlLayer::judgeMousePositionIsInMap()
{
	return (_plantsPosition.x >= 0 && _plantsPosition.x <= 8 && _plantsPosition.y >= 0 && _plantsPosition.y <= 4) ? true : false;
}

bool GSControlLayer::judgeMousePositionIsCanPlant()
{
	return (_gameMapInformation->plantsMap[static_cast<int>(_plantsPosition.y)][static_cast<int>(_plantsPosition.x)] != CAN_NOT_PLANT /* 不等于不能种植 （在能种植的范围在内）*/
		&& _gameMapInformation->plantsMap[static_cast<int>(_plantsPosition.y)][static_cast<int>(_plantsPosition.x)] == NO_PLANTS)     /* 能种植的范围内还没有种植 */
		? true : false;
}

bool GSControlLayer::judgeMousePositionHavePlant()
{
	return (_gameMapInformation->plantsMap[static_cast<int>(_plantsPosition.y)][static_cast<int>(_plantsPosition.x)] != CAN_NOT_PLANT
		&& _gameMapInformation->plantsMap[static_cast<int>(_plantsPosition.y)][static_cast<int>(_plantsPosition.x)] != NO_PLANTS)
		? true : false;
}

void GSControlLayer::removePreviewPlant()
{
	/* 移除预览植物 */
	_plantPreviewImage->removeFromParent();
	_plantCurImage->removeFromParent();
	buttonLayerInformation->mouseSelectImage->isSelectPlants = false;
	CURSOR_VISIBLE(true)
}

void GSControlLayer::removeShovel()
{
	buttonLayerInformation->mouseSelectImage->isSelectShovel = false;
	_director->getOpenGLView()->setCursor(_global->userInformation->getImagePath().find("cursor")->second, Point::ANCHOR_TOP_LEFT);
}

void GSControlLayer::recoveryPlantsColor()
{
	for (int i = 0; i < _gameMapInformation->rowNumbers; i++)
	{
		for (int j = 0; j < _gameMapInformation->columnNumbers; j++)
		{
			if (_gameMapInformation->plantsMap[i][j] != CAN_NOT_PLANT && _gameMapInformation->plantsMap[i][j] != NO_PLANTS)
			{
				auto plant = _animationLayer->getChildByTag(SET_TAG(Vec2(j, i)));
				if (plant)
				{
					plant->setColor(Color3B::WHITE);
				}
			}
		}
	}
}

void GSControlLayer::judgeLevelIsFinished()
{
	/* 关卡结束 */
	if (ZombiesGroup.size() <= 0 && _zombiesAppearControl->getZombiesAppearFrequency() >= _openLevelData->readLevelData(_openLevelData->getLevelNumber())->getZombiesFrequency())
	{
		CURSOR_VISIBLE(true)

		setGameEnd();

		auto judgeUserWin = new GSGameResultJudgement();
		if (judgeUserWin->judgeUserIsWin() == GameTypes::None)
		{
			if (_global->userInformation->getCurrentPlayLevels() >= 52)
			{
				_director->getInstance()->pushScene(TransitionFade::create(0.5f, GameEasterEggs::createScene()));
			}
			_gameEndShieldLayer->successfullEntry();
		}
		else
		{
			_gameEndShieldLayer->breakThrough(judgeUserWin->judgeUserIsWin()); /* 闯关失败 */
		}
	}
}

void GSControlLayer::setGameEnd()
{
	unschedule("mainUpdate");
	unschedule("zombiesComing");
	SunFlower::stopSun();
	Plants::stopPlantsAllAction();
	animationLayerInformation->stopRandomSun();

	_director->getScheduler()->setTimeScale(1.0f);

	_gameEndShieldLayer = GSGameEndLayer::create();
	_director->getRunningScene()->addChild(_gameEndShieldLayer, 10);
}

void GSControlLayer::mouseMoveControl()
{
	/* 如果鼠标选择了植物 */
	if (buttonLayerInformation->mouseSelectImage->isSelectPlants)
	{
		if (_gameMapInformation->plantsMap[static_cast<int>(_plantsPosition.y)][static_cast<int>(_plantsPosition.x)] != NO_PLANTS)
		{
			_plantPreviewImage->setPosition(Vec2(-1000, -1000));
		}
		else
		{
			_plantPreviewImage->setPosition(Vec2(GRASS_POSITION_LEFT + 122 * _plantsPosition.x + 60, 110 + 138 * (_plantsPosition.y + 1) - 60));
		}
		_plantCurImage->setPosition(_cur);
	}

	/* 鼠标上有铲子 */
	if (buttonLayerInformation->mouseSelectImage->isSelectShovel)
	{
		if (!_animationLayer)
		{
			_animationLayer = _director->getRunningScene()->getChildByName("animationLayer");
		}

		/* 循环把植物恢复到原来的颜色 */
		recoveryPlantsColor();

		if (judgeMousePositionIsInMap() && judgeMousePositionHavePlant())  /* 如果在地图范围内 && 种有植物 */
		{
			auto plant = _animationLayer->getChildByTag(SET_TAG(_plantsPosition));
			if (plant)
			{
				plant->setColor(Color3B(100, 100, 100));
			}
		}
	}
}

void GSControlLayer::mouseDownControl(EventMouse* eventmouse)
{
	if (eventmouse->getMouseButton() == EventMouse::MouseButton::BUTTON_RIGHT) /* 如果是右键按下 */
	{	
		if (buttonLayerInformation->mouseSelectImage->isSelectPlants)/* 鼠标上有植物 */
		{
			AudioEngine::setVolume(AudioEngine::play2d(_global->userInformation->getMusicPath().find("tap2")->second), _global->userInformation->getSoundEffectVolume());
			buttonLayerInformation->plantsCards[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)].progressTimer->setPercentage(0);
			buttonLayerInformation->plantsCards[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)].plantsCards->setColor(Color3B::WHITE);

			/* 加上所需的阳光数并更新 */
			_global->userInformation->setSunNumbers(_global->userInformation->getSunNumbers() + buttonLayerInformation->plantsInformation->PlantsNeedSunNumbers[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)]);
			informationLayerInformation->updateSunNumbers();

			/* 植物要求更新 */
			backgroundLayerInformation->gameType->updateRequirementNumbers("植物数量增加");

			removePreviewPlant();
		}

		if (buttonLayerInformation->mouseSelectImage->isSelectShovel) /* 鼠标上有铲子 */
		{
			removeShovel();
			recoveryPlantsColor();
		}
	}

	if (eventmouse->getMouseButton() == EventMouse::MouseButton::BUTTON_LEFT) /* 如果是左键按下 */
	{
		if (buttonLayerInformation->mouseSelectImage->isSelectPlants)
		{
			if (judgeMousePositionIsInMap() && judgeMousePositionIsCanPlant()) /* 如果在地图范围内 && 可以种植植物 */
			{
				/* 记录使用植物数量 */
				UserDefault::getInstance()->setIntegerForKey("USEPLANTSNUMBERS", ++_global->userInformation->getUsePlantsNumbers());

				/* 种植植物 */
				animationLayerInformation->plantPlants();

				/* 地图记录种植的植物 */
				_gameMapInformation->plantsMap[static_cast<int>(_plantsPosition.y)][static_cast<int>(_plantsPosition.x)] = static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId);

				/* 设置倒计时并且按钮不可用 */
				int plantsTag = static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId);
				buttonLayerInformation->plantsCards[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)].timeBarIsFinished = false;
				buttonLayerInformation->plantsCards[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)].plantsCards->setEnabled(false);
				buttonLayerInformation->plantsCards[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)].plantsCards->setColor(Color3B::GRAY);
				buttonLayerInformation->plantsCards[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)].progressTimer->runAction(
					Sequence::create(ProgressFromTo::create(buttonLayerInformation->plantsInformation->PlantsCoolTime[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)], 100, 0),
						CallFunc::create([=]() { buttonLayerInformation->plantsCards[plantsTag].timeBarIsFinished = true; }), nullptr)
				);
				removePreviewPlant();
			}
			else
			{
				if (_cur.x > CARD_BAR_RIGHT)
				{
					AudioEngine::setVolume(AudioEngine::play2d(_global->userInformation->getMusicPath().find("buzzer")->second), _global->userInformation->getSoundEffectVolume());
					/* 卡牌颜色恢复 */
					buttonLayerInformation->plantsCards[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)].progressTimer->setPercentage(0);
					buttonLayerInformation->plantsCards[static_cast<int>(buttonLayerInformation->mouseSelectImage->selectPlantsId)].plantsCards->setColor(Color3B::WHITE);

					/* 提示信息 */
					informationLayerInformation->createPromptText();

					removePreviewPlant();
				}
			}
		}
		if (buttonLayerInformation->mouseSelectImage->isSelectShovel) /* 鼠标上有铲子 */
		{
			AudioEngine::setVolume(AudioEngine::play2d(_global->userInformation->getMusicPath().find("plant2")->second), _global->userInformation->getSoundEffectVolume());
			if (judgeMousePositionIsInMap() && judgeMousePositionHavePlant())    /* 如果在地图范围内 && 种有植物 */
			{
				animationLayerInformation->deletePlants();/* 铲除植物 */
			}
			removeShovel();
		}
	}
}


ZombiesAppearControl::ZombiesAppearControl():
	_zombiesAppearFrequency(0)
,	_lastFrequencyZombiesWasDeath(false)
,	_isBegin(false)
,	_isShowWords(false)
,	_time(0)
,   _openLevelData(OpenLevelData::getInstance())
{
	_random.seed(time(nullptr));
}

ZombiesAppearControl::~ZombiesAppearControl()
{
}

ZombiesType ZombiesAppearControl::createDifferentTypeZombies(const int& zombiesAppearFrequency)
{
	int number = rand() % 100;
	int sum = 0, i = -1;
	auto zombiesTypeProbability = _openLevelData->readLevelData(_openLevelData->getLevelNumber())->getZombiesTypeProbabilityFrequency().at(zombiesAppearFrequency);
	for (auto& probability : zombiesTypeProbability)
	{
		++i;
		sum += probability;
		if (number < sum)
		{
			return static_cast<ZombiesType>(_openLevelData->readLevelData(_openLevelData->getLevelNumber())->getZombiesType().at(i));
		}
	}
	return ZombiesType::None;
}

int ZombiesAppearControl::getZombiesNumbersForAppearFrequency(const int& ZombiesAppearFrequency)
{
	const int number = _openLevelData->readLevelData(_openLevelData->getLevelNumber())->getZombiesNumbers().at(ZombiesAppearFrequency);
	return number + rand() % number;
}

int ZombiesAppearControl::getEqualProbabilityForRow()
{
	uniform_int_distribution<unsigned>number(0, 4);

	if (_judgeZombieRow.size() == 5)
	{
		_judgeZombieRow.clear();
	}

	int row;
	do
	{
		row = number(_random);
	} while (_judgeZombieRow.count(row));

	_judgeZombieRow.insert(row);
	
	return _zombiesPosition[row];
}