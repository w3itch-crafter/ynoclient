/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#include <sstream>
#include "game_battleaction.h"
#include "game_battler.h"
#include "game_party_base.h"
#include "game_enemy.h"
#include "game_temp.h"
#include "main_data.h"
#include "game_message.h"
#include "game_actor.h"
#include "game_system.h"
#include "game_battle.h"
#include "spriteset_battle.h"

Game_BattleAction::ActionBase::ActionBase(Game_Battler* source) :
	state(State_PreAction),
	animation(NULL),
	wait(30),
	source(source) {
		// no-op
}

void Game_BattleAction::ActionBase::PreAction() {
	Game_Message::texts.push_back("\r");
	Game_Message::texts.push_back(source->GetName() + Data::terms.attacking);
}

void Game_BattleAction::ActionBase::PlayAnimation(BattleAnimation* animation) {
	this->animation = animation;
}

Game_Battler* Game_BattleAction::ActionBase::GetSource() {
	return source;
}

Game_BattleAction::SingleTargetAction::SingleTargetAction(Game_Battler* source, Game_Battler* target) :
	ActionBase(source), target(target) {
	// no-op
}

bool Game_BattleAction::SingleTargetAction::Execute() {
	if (animation) {
		if (!animation->GetVisible())
			animation->SetVisible(true);

		if (animation->GetFrame() >= animation->GetFrames()) {
			delete animation;
			animation = NULL;
		} else {
			animation->Update();
		}
		return false;
	}

	//if (Game_Message::message_waiting) {
	//	return false;
	//}

	switch(state) {
		case State_PreAction:
			PreAction();
			state = State_Action;
			break;
		case State_Action:
			Action();
			state = State_PostAction;
			break;
		case State_PostAction:
			if (wait--) {
				return false;
			}
			wait = 30;

			PostAction();
			state = algorithm->GetAffectedConditions().empty() ? State_Finished : State_ResultAction;
			break;
		case State_ResultAction:
			if (wait--) {
				return false;
			}
			wait = 30;

			ResultAction();
			state = State_Finished;
			break;
		case State_Finished:
			if (wait--) {
				return false;
			}
			wait = 30;

			return true;
	}

	return false;
}

void Game_BattleAction::SingleTargetAction::ResultAction() {
	if (target->GetSignificantState()->ID == 1) {
		Sprite_Battler* target_sprite = Game_Battle::GetSpriteset().FindBattler(target);
		if (target_sprite) {
			target_sprite->SetAnimationState(Sprite_Battler::Dead);
			Game_System::SePlay(Data::system.enemy_death_se);
		}
	}

	if (target->GetType() == Game_Battler::Type_Ally) {
		Game_Message::texts.push_back(target->GetName() + target->GetSignificantState()->message_actor);
	} else {
		Game_Message::texts.push_back(target->GetName() + target->GetSignificantState()->message_enemy);
	}
}

Game_BattleAction::PartyTargetAction::PartyTargetAction(Game_Battler* source, Game_Party_Base* target) :
	ActionBase(source), target(target) {

	target->GetAliveBattlers(alive);
	current_target = alive.begin();
}

bool Game_BattleAction::PartyTargetAction::Execute() {
	if (animation) {
		if (!animation->GetVisible())
			animation->SetVisible(true);

		if (animation->GetFrame() >= animation->GetFrames()) {
			delete animation;
			animation = NULL;
		} else {
			animation->Update();
		}
		return false;
	}

	//if (Game_Message::message_waiting) {
	//	return false;
	//}


	switch (state) {
		case State_PreAction:
			PreAction();
			state = State_Action;
			break;
		case State_Action:
			Action();
			state = State_PostAction;
			break;
		case State_PostAction:
			if (wait--) {
				return false;
			}
			wait = 30;

			PostAction();
			state = algorithm->GetAffectedConditions().empty() ? State_Finished : State_ResultAction;
			break;
		case State_ResultAction:
			if (wait--) {
				return false;
			}
			wait = 30;

			ResultAction();
			state = State_Finished;
			break;
		case State_Finished:
			if (wait--) {
				return false;
			}
			wait = 30;

			++current_target;
			state = State_PreAction;
			return current_target == alive.end();
	}

	return false;
}

void Game_BattleAction::PartyTargetAction::ResultAction() {
	if ((*current_target)->GetSignificantState()->ID == 1) {
		Sprite_Battler* target_sprite = Game_Battle::GetSpriteset().FindBattler(*current_target);
		if (target_sprite) {
			target_sprite->SetAnimationState(Sprite_Battler::Dead);
			Game_System::SePlay(Data::system.enemy_death_se);
		}
	}

	if ((*current_target)->GetType() == Game_Battler::Type_Ally) {
		Game_Message::texts.push_back((*current_target)->GetName() + (*current_target)->GetSignificantState()->message_actor);
	} else {
		Game_Message::texts.push_back((*current_target)->GetName() + (*current_target)->GetSignificantState()->message_enemy);
	}
}

Game_BattleAction::AttackSingleNormal::AttackSingleNormal(Game_Battler* source, Game_Battler* target) :
	SingleTargetAction(source, target)
{
	algorithm.reset(new Game_BattleAlgorithm::Normal(source, target));
}

void Game_BattleAction::AttackSingleNormal::Action() {
	if (target->IsDead()) {
		// Repoint to a different target if the selected one is dead
		target = target->GetParty().GetRandomAliveBattler();
	}

	Sprite_Battler* source_sprite = Game_Battle::GetSpriteset().FindBattler(source);
	if (source_sprite) {
		source_sprite->SetAnimationState(Sprite_Battler::SkillUse);
	}

	algorithm->Execute();

	if (algorithm->GetAnimation()) {
		PlayAnimation(new BattleAnimation(target->GetBattleX(), target->GetBattleY(), algorithm->GetAnimation()));
	}

	if (algorithm->GetAffectedHp()) {
		target->ChangeHp(-*algorithm->GetAffectedHp());
	}
}

void Game_BattleAction::AttackSingleNormal::PostAction() {
	bool target_is_ally = target->GetType() == Game_Battler::Type_Ally;

	std::stringstream ss;
	ss << target->GetName();

	if (!algorithm->GetAffectedHp()) {
		ss << Data::terms.dodge;
		Game_System::SePlay(Data::system.dodge_se);
	} else {
		if (*algorithm->GetAffectedHp() == 0) {
			ss << (target_is_ally ?
				Data::terms.actor_undamaged :
				Data::terms.enemy_undamaged);
		} else {
			ss << " " << *algorithm->GetAffectedHp() << (target_is_ally ?
				Data::terms.actor_damaged :
				Data::terms.enemy_damaged);
		}
		Game_System::SePlay(target_is_ally ?
			Data::system.actor_damaged_se :
			Data::system.enemy_damaged_se);

		Sprite_Battler* target_sprite = Game_Battle::GetSpriteset().FindBattler(target);
		if (target_sprite) {
			target_sprite->SetAnimationState(Sprite_Battler::Damage);
		}
	}

	Game_Message::texts.push_back(ss.str());
}

Game_BattleAction::AttackPartyNormal::AttackPartyNormal(Game_Battler* source, Game_Party_Base* target) :
	PartyTargetAction(source, target) {
	// no-op
}

void Game_BattleAction::AttackPartyNormal::Action() {
	
}

void Game_BattleAction::AttackPartyNormal::PostAction() {

}

Game_BattleAction::AttackSingleSkill::AttackSingleSkill(Game_Battler* source, Game_Battler* target, RPG::Skill* skill) :
	SingleTargetAction(source, target), skill(skill) {
	// no-op
}

void Game_BattleAction::AttackSingleSkill::Action() {
	// Todo
}

void Game_BattleAction::AttackSingleSkill::PostAction() {

}

Game_BattleAction::AttackPartySkill::AttackPartySkill(Game_Battler* source, Game_Party_Base* target, RPG::Skill* skill) :
	PartyTargetAction(source, target), skill(skill) {
	// no-op
}

void Game_BattleAction::AttackPartySkill::Action() {
	algorithm.reset(new Game_BattleAlgorithm::Skill(source, *current_target, *skill));

	Sprite_Battler* source_sprite = Game_Battle::GetSpriteset().FindBattler(source);
	if (source_sprite) {
		source_sprite->SetAnimationState(Sprite_Battler::SkillUse);
	}

	algorithm->Execute();

	if (current_target == alive.begin()) {
		source->SetSp(source->GetSp() - skill->sp_cost);
		if (algorithm->GetAnimation()) {
			PlayAnimation(new BattleAnimation(160, 120,
				algorithm->GetAnimation()));
		}
	}

	if (algorithm->GetAffectedHp()) {
		(*current_target)->ChangeHp(-*algorithm->GetAffectedHp());
	}
}

void Game_BattleAction::AttackPartySkill::PostAction() {
	bool target_is_ally = (*current_target)->GetType() == Game_Battler::Type_Ally;

	std::stringstream ss;
	ss << (*current_target)->GetName();

	if (!algorithm->GetAffectedHp()) {
		ss << Data::terms.dodge;
		Game_System::SePlay(Data::system.dodge_se);
	} else {
		if (*algorithm->GetAffectedHp() == 0) {
			ss << (target_is_ally ?
				Data::terms.actor_undamaged :
			Data::terms.enemy_undamaged);
		} else {
			ss << " " << *algorithm->GetAffectedHp() << (target_is_ally ?
				Data::terms.actor_damaged :
			Data::terms.enemy_damaged);
		}
		Game_System::SePlay(target_is_ally ?
			Data::system.actor_damaged_se :
		Data::system.enemy_damaged_se);

		Sprite_Battler* target_sprite = Game_Battle::GetSpriteset().FindBattler((*current_target));
		if (target_sprite) {
			target_sprite->SetAnimationState(Sprite_Battler::Damage);
		}
	}

	Game_Message::texts.push_back(ss.str());
}
