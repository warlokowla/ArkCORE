/*
* Copyright (C) 2011-2012 ProjectStudioMirage <http://www.studio-mirage.fr/>
* Copyright (C) 2011-2012 https://github.com/Asardial
*/

#include "ScriptMgr.h"
#include "grimbatol.h"

// ToDo move the Yells to the DB
#define SAY_AGGRO "The darkest days are still ahead!"
#define SAY_DEATH "Ywaq maq oou; ywaq maq ssaggh. Yawq ma shg'fhn."
#define SAY_SUMMON "Come, suffering... Enter, chaos!"
#define SAY_SHADOW_GALE "F'lakh ghet! The shadow's hunger cannot be sated!"
#define SAY_GALE "F'lakh ghet! The shadow's hunger cannot be sated!"
#define SAY_KILL "More flesh for the offering!"

enum Spells
{
	// Erudax
	SPELL_ENFEEBLING_BLOW                     = 75789,
	SPELL_SHADOW_GALE_VISUAL			= 75664,

	// (litte hole at the caster, it is a pre visual aura of shadow gale) 
	SPELL_SHADOW_GALE_SPEED_TRIGGER		= 75675,
	SPELL_SHADOW_GALE_DEBUFF			= 75694,

	// Spawns 1 (NH - 40600) or 2 (HC - 48844) Faceless 
	SPELL_SPAWN_FACELESS				= 75704,
	SPELL_TWILIGHT_PORTAL_VISUAL		= 95716,

	// In this Script is is casted by the Faceless itself
	SPELL_SHIELD_OF_NIGHTMARE			= 75809,

	SPELL_BINDING_SHADOWS                     = 79466, // Wowhead is wrong

	// Faceless
	SPELL_UMBRAL_MENDING				= 79467, // Wowhead is wrong
	SPELL_TWILIGHT_CORRUPTION_DOT		= 93613,
	SPELL_TWILIGHT_CORRUPTION_VISUAL	       = 75755, //91049,

	// Alexstraszas Eggs
	SPELL_SUMMON_TWILIGHT_HATCHLINGS          = 91058,
};

enum Events
{
	EVENT_ENFEEBLING_BLOW					= 1,
	EVENT_SHADOW_GALE						= 2,
	EVENT_SUMMON_FACELESS					= 3,
	EVENT_REMOVE_TWILIGHT_PORTAL			= 4,
	EVENT_CAST_SHIELD_OF_NIGHTMARE_DELAY	= 5,
	EVENT_BINDING_SHADOWS					= 6,

	EVENT_TRIGGER_GALE_CHECK_PLAYERS		= 7,
};

enum Points
{
	POINT_FACELESS_IS_AT_AN_EGG = 1,
	POINT_ERUDAX_IS_AT_STALKER	= 2,
};

class boss_erudax: public CreatureScript
{
public: 
	boss_erudax() : CreatureScript("boss_erudax") { } 

	CreatureAI* GetAI(Creature* creature) const
	{
		return new boss_erudaxAI (creature);
	}

	struct boss_erudaxAI : public ScriptedAI
	{
		boss_erudaxAI(Creature* pCreature) : ScriptedAI(pCreature), ShouldSummonAdds(false)
		{
			instance = pCreature->GetInstanceScript();
		}

		Unit* FacelessPortalStalker;
		Unit* ShadowGaleTrigger;

		InstanceScript* instance;
		EventMap events;

		bool ShouldSummonAdds;

		void Reset()
		{
			if (instance)
			me->GetMotionMaster()->MoveTargetedHome();

			events.Reset();

			ResetMinions();
			RemoveShadowGaleDebuffFromPlayers();
		}

		void EnterCombat(Unit* /*who*/) 
		{
			if (instance)
			ShouldSummonAdds = false;

			// Fixes wrong behaviour of Erudax if the boss was respawned
			me->SetReactState(REACT_AGGRESSIVE);
			me->GetMotionMaster()->Clear();
			me->GetMotionMaster()->MoveChase(me->GetVictim());

			events.ScheduleEvent(EVENT_ENFEEBLING_BLOW, 4000);

			events.ScheduleEvent(EVENT_BINDING_SHADOWS, 9000);

			events.ScheduleEvent(EVENT_SHADOW_GALE, 20000);

			me->MonsterYell(SAY_AGGRO, LANG_UNIVERSAL, NULL);

			// The Position of the Portal Stalker is the Summon Position of the Adds
			FacelessPortalStalker = me->SummonCreature(NPC_FACELESS_PORTAL_STALKER,-641.515f,-827.8f,235.5f,3.069f,TEMPSUMMON_MANUAL_DESPAWN);

			FacelessPortalStalker->SetFlag(UNIT_FIELD_FLAGS,UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_NOT_SELECTABLE);
		}

		void UpdateAI(const uint32 diff)
		{
			if (!UpdateVictim() || me->HasUnitState(UNIT_STATE_CASTING))
				return;

			if(ShouldSummonAdds)
			{
				// Despawns the Stalker
				DespawnCreatures(NPC_SHADOW_GALE_STALKER);
				RemoveShadowGaleDebuffFromPlayers();

				me->SetReactState(REACT_AGGRESSIVE);
				me->GetMotionMaster()->Clear();
				me->GetMotionMaster()->MoveChase(me->GetVictim());

				if ((rand()%2))
					me->MonsterYell(SAY_SUMMON, LANG_UNIVERSAL, NULL);

				//Adds a visual portal effect to the Stalker
                            if (FacelessPortalStalker)
				FacelessPortalStalker->GetAI()->DoCast(FacelessPortalStalker,SPELL_TWILIGHT_PORTAL_VISUAL,true);
				events.ScheduleEvent(EVENT_REMOVE_TWILIGHT_PORTAL, 7000);

				//Summons Faceless over the Spell
                            if (FacelessPortalStalker)
				FacelessPortalStalker->GetAI()->DoCast(FacelessPortalStalker,SPELL_SPAWN_FACELESS,true);

				ShouldSummonAdds = false;

				// DBM says that the Spell has 40s CD
				events.ScheduleEvent(EVENT_SHADOW_GALE, urand(40000,44000));
			}

			events.Update(diff);

			while (uint32 eventId = events.ExecuteEvent())
			{
				switch (eventId)
				{

				case EVENT_ENFEEBLING_BLOW:
					DoCastVictim(SPELL_ENFEEBLING_BLOW);
					events.ScheduleEvent(EVENT_ENFEEBLING_BLOW, urand(19000,24000));
					break;

				case EVENT_SHADOW_GALE:
                                   if (Unit* target = SelectTarget(SELECT_TARGET_RANDOM, 0, 0, true, 0))
                                   {
                                       ShadowGaleTrigger = me->SummonCreature(NPC_SHADOW_GALE_STALKER, target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), target->GetOrientation(), TEMPSUMMON_CORPSE_DESPAWN);
                                       me->SetReactState(REACT_PASSIVE);
                                       me->GetMotionMaster()->MovePoint(POINT_ERUDAX_IS_AT_STALKER, -739.665f, -827.024f, 232.412f);
                                       me->MonsterYell(SAY_GALE, LANG_UNIVERSAL, NULL);
                                   }
					break;

				case EVENT_REMOVE_TWILIGHT_PORTAL:
					//Removes Portal effect from Stalker
					FacelessPortalStalker->RemoveAllAuras();
					break;

				case EVENT_BINDING_SHADOWS:

					if (Unit* tempTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 500.0f, true))
						DoCast(tempTarget,SPELL_BINDING_SHADOWS);
					events.ScheduleEvent(EVENT_BINDING_SHADOWS, urand(12000,17000));
					break;

				default:
					break;
				}
			}

			DoMeleeAttackIfReady();
		}

		void KilledUnit(Unit* victim)
		{
			me->MonsterYell(SAY_KILL, LANG_UNIVERSAL, NULL);
		}

		virtual void JustReachedHome()
		{
			ResetMinions();
		}


		void JustDied(Unit* /*killer*/)
		{	
			ResetMinions();

			RemoveShadowGaleDebuffFromPlayers();

			me->MonsterYell(SAY_DEATH, LANG_UNIVERSAL, NULL);
		}

		void JustSummoned(Creature* summon)
		{
			summon->setActive(true);
		}

		void MovementInform(uint32 type, uint32 id)
		{
			if (type == POINT_MOTION_TYPE)
			{
				switch (id)
				{
				case POINT_ERUDAX_IS_AT_STALKER:

					// if Erudax is not at the Stalkers poision while he is casting
					// the Casting Effect would not displayed right				
					DoCast(SPELL_SHADOW_GALE_VISUAL);
					ShouldSummonAdds = true;

					break;

				default:
					break;
				}
			}
		}

	private:
		void ResetMinions()
		{
			DespawnCreatures(NPC_FACELESS);
			DespawnCreatures(NPC_FACELESS_HC);
			DespawnCreatures(NPC_FACELESS_PORTAL_STALKER);
			DespawnCreatures(NPC_TWILIGHT_HATCHLING);
			DespawnCreatures(NPC_SHADOW_GALE_STALKER);
			RespawnEggs();
		}

		void DespawnCreatures(uint32 entry)
		{
			std::list<Creature*> creatures;
			GetCreatureListWithEntryInGrid(creatures, me, entry, 1000.0f);

			if (creatures.empty())
				return;

			for (std::list<Creature*>::iterator iter = creatures.begin(); iter != creatures.end(); ++iter)
				(*iter)->DespawnOrUnsummon();
		}

		void RespawnEggs()
		{
			std::list<Creature*> creatures;
			GetCreatureListWithEntryInGrid(creatures, me, NPC_ALEXSTRASZAS_EGG, 1000.0f);

			if (creatures.empty())
				return;

			for (std::list<Creature*>::iterator iter = creatures.begin(); iter != creatures.end(); ++iter)
			{	
				if((*iter)->IsDead())
					(*iter)->Respawn();

				(*iter)->SetHealth(77500);
				(*iter)->SetMaxHealth(77500);
			}
		}

		void RemoveShadowGaleDebuffFromPlayers()
		{
			Map::PlayerList const &PlayerList =  me->GetMap()->GetPlayers();

			if (!PlayerList.isEmpty())
			{
				for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
						i->GetSource()->RemoveAura(SPELL_SHADOW_GALE_DEBUFF);
			}
		}
	};
};

class npc_faceless : public CreatureScript
{
public:
	npc_faceless() : CreatureScript("npc_faceless") { }

	CreatureAI* GetAI(Creature* creature) const
	{
		return new npc_facelessAI (creature);
	}

	struct npc_facelessAI : public ScriptedAI
	{
		npc_facelessAI(Creature* creature) : ScriptedAI(creature), pTarget(NULL), isAtAnEgg(false), isCastingUmbraMending (false) {}

		Creature* pTarget;
		Unit* pErudax;

		bool isAtAnEgg;
		bool isCastingUmbraMending;

		EventMap events;

		void IsSummonedBy(Unit* summoner)
		{
			pTarget = GetRandomEgg();

			DoZoneInCombat();

			if(me->GetMap()->IsHeroic())
				events.ScheduleEvent(EVENT_CAST_SHIELD_OF_NIGHTMARE_DELAY, 3000);

			if(pTarget != NULL)
			{
				me->GetMotionMaster()->MovePoint(POINT_FACELESS_IS_AT_AN_EGG,pTarget->GetPositionX()-4.0f,pTarget->GetPositionY()-4.0f,pTarget->GetPositionZ());
			}

			me->SetReactState(REACT_PASSIVE); // That the Faceless are not running to Players while running to Eggs
		}

		void UpdateAI(const uint32 diff)
		{	
			if (pTarget == NULL || !isAtAnEgg || me->HasUnitState(UNIT_STATE_CASTING))
				return;

			events.Update(diff);

			while (uint32 eventId = events.ExecuteEvent())
			{
				switch (eventId)
				{
				case EVENT_CAST_SHIELD_OF_NIGHTMARE_DELAY:
					DoCast(me, SPELL_SHIELD_OF_NIGHTMARE, true);
					break;

				default:
					break;
				}
			}

			if(isCastingUmbraMending)
			{	// If the Egg is Death and Umbra Mending was casted go to the next Egg

				pTarget = GetNextEgg();

				if(pTarget != NULL) // Solves Crashes if the Faceless killed all eggs
					me->GetMotionMaster()->MovePoint(POINT_FACELESS_IS_AT_AN_EGG,pTarget->GetPositionX(),pTarget->GetPositionY(),pTarget->GetPositionZ());

				isAtAnEgg = false;
				isCastingUmbraMending = false;

				return;
			}

			if(pTarget->IsDead())
			{
				if(Unit* pErudax = me->FindNearestCreature(BOSS_ERUDAX,1000.0f, true))
					DoCast(pErudax, SPELL_UMBRAL_MENDING,false);

				isCastingUmbraMending = true;

				return;
			}

			pTarget->AI()->DoZoneInCombat();

			DoCast(pTarget,SPELL_TWILIGHT_CORRUPTION_DOT,true);
			DoCast(pTarget,SPELL_TWILIGHT_CORRUPTION_VISUAL,true);
		}

		void MovementInform(uint32 type, uint32 id)
		{
			if (type == POINT_MOTION_TYPE)
			{
				switch (id)
				{
				case POINT_FACELESS_IS_AT_AN_EGG:
					isAtAnEgg = true;
					break;

				default:
					break;
				}
			}
		}

		void JustDied(Unit* killer)
		{	// Removes the Dot of the Egg if the Faceless dies
			if(isAtAnEgg && pTarget->IsAlive())
				pTarget->RemoveAllAuras();
		}

	private:
		Creature* GetRandomEgg()
		{	
			// I know that this is looking strange but it works! ^^

			std::list<Creature*> creatures;
			GetCreatureListWithEntryInGrid(creatures, me, NPC_ALEXSTRASZAS_EGG, 300.0f);


			if (creatures.empty())
				return GetNextEgg();

			uint32 c = 0;
			uint32 r = urand(0,creatures.size());

			for (std::list<Creature*>::iterator iter = creatures.begin(); iter != creatures.end(); ++iter)
			{
				if (c == r)
					return (*iter);

				c++;
			}

			return GetNextEgg();
		}

		inline Creature* GetNextEgg()
		{
			return me->FindNearestCreature(NPC_ALEXSTRASZAS_EGG,1000.0f, true);
		}
	};
};

class npc_alexstraszas_eggs : public CreatureScript
{
public:
	npc_alexstraszas_eggs() : CreatureScript("npc_alexstraszas_eggs") { }

	CreatureAI* GetAI(Creature* creature) const
	{
		return new npc_alexstraszas_eggsAI (creature);
	}

	struct npc_alexstraszas_eggsAI : public ScriptedAI
	{
		npc_alexstraszas_eggsAI(Creature* creature) : ScriptedAI(creature)
		{
			me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_NOT_SELECTABLE);

			me->SetReactState(REACT_PASSIVE);
		}

		void JustDied(Unit* killer)
		{	// Summon Twilight Hatchlings

			// Despawn of the Hatchlings is handled by Erudax
			// The behaviour of the hatchlings is handled through SmartAI

			DoCastAOE(SPELL_SUMMON_TWILIGHT_HATCHLINGS, true);
		}

		void JustSummoned(Creature* summon)
		{
			summon->setActive(true);
			summon->AI()->DoZoneInCombat();

			if (GetPlayerAtMinimumRange(0))
				summon->Attack(GetPlayerAtMinimumRange(0), true);
		}
	};
};

class npc_shadow_gale_stalker : public CreatureScript
{
public:
	npc_shadow_gale_stalker() : CreatureScript("npc_shadow_gale_stalker") { }

	CreatureAI* GetAI(Creature* creature) const
	{
		return new npc_shadow_gale_stalkerAI (creature);
	}

	struct npc_shadow_gale_stalkerAI : public ScriptedAI
	{
		npc_shadow_gale_stalkerAI(Creature* creature) : ScriptedAI(creature), VisualEffectCasted(false) {}

		Unit* pErudax;
		EventMap events;
		bool VisualEffectCasted;

		void IsSummonedBy(Unit* summoner)
		{
			pErudax = summoner;
			me->SetFlag(UNIT_FIELD_FLAGS,UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_NOT_SELECTABLE);
			DoCastAOE(SPELL_SHADOW_GALE_SPEED_TRIGGER);
		}

		void UpdateAI(const uint32 diff)
		{	
			if(VisualEffectCasted)
			{
				events.Update(diff);

				while (uint32 eventId = events.ExecuteEvent())
				{
					switch (eventId)
					{
					case EVENT_TRIGGER_GALE_CHECK_PLAYERS:

						Map::PlayerList const &PlayerList =  me->GetMap()->GetPlayers();

						if (!PlayerList.isEmpty())
						{
							for (Map::PlayerList::const_iterator i = PlayerList.begin(); i != PlayerList.end(); ++i)
								if(me->GetDistance(i->GetSource()) >= 3)
								{
									// ToDo Add Debuff and Deal damage
									if(!i->GetSource()->HasAura(SPELL_SHADOW_GALE_DEBUFF))
										me->CastSpell(i->GetSource(), SPELL_SHADOW_GALE_DEBUFF, true);
								}else
									i->GetSource()->RemoveAura(SPELL_SHADOW_GALE_DEBUFF);
						}

						events.ScheduleEvent(EVENT_TRIGGER_GALE_CHECK_PLAYERS, 1000);
						break;
					}
				}
			}

			if (me->HasUnitState(UNIT_STATE_CASTING))
				return;

			if(!VisualEffectCasted)
			{
				VisualEffectCasted = true;
				events.ScheduleEvent(EVENT_TRIGGER_GALE_CHECK_PLAYERS, 1000);
			}
		}
	};
};

void AddSC_boss_erudax() 
{
	new boss_erudax();
	new npc_faceless();
	new npc_alexstraszas_eggs();
	new npc_shadow_gale_stalker();
}