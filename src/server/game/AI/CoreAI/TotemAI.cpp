/*
 * Copyright (C) 2008-2012 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "TotemAI.h"
#include "Totem.h"
#include "Creature.h"
#include "DBCStores.h"
#include "ObjectAccessor.h"
#include "SpellMgr.h"

#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

int TotemAI::Permissible(Creature const* creature)
{
    if (creature->isTotem())
        return PERMIT_BASE_PROACTIVE;

    return PERMIT_BASE_NO;
}

TotemAI::TotemAI(Creature* c) : CreatureAI(c), i_victimGuid(0)
{
    ASSERT(c->isTotem());
}

void TotemAI::MoveInLineOfSight(Unit* /*who*/)
{
}

void TotemAI::EnterEvadeMode()
{
    me->CombatStop(true);
}

void TotemAI::UpdateAI(uint32 const /*diff*/)
{
    if (me->ToTotem()->GetTotemType() != TOTEM_ACTIVE)
        return;

    if (!me->isAlive())
        return;

    // pointer to appropriate target if found any
    Unit* victim = i_victimGuid ? ObjectAccessor::GetUnit(*me, i_victimGuid) : NULL;

    if (me->IsNonMeleeSpellCasted(false))
    {
        if (victim && victim->HasCrowdControlAura())
            victim = NULL;
        else
            return;
    }

    // Search spell
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(me->ToTotem()->GetSpell());
    if (!spellInfo)
        return;

    // Get spell range
    float max_range = spellInfo->GetMaxRange(false);

    // SPELLMOD_RANGE not applied in this place just because not existence range mods for attacking totems

    // Search victim if no, not attackable, or out of range, or friendly (possible in case duel end)
    if (!victim ||
        !victim->isTargetableForAttack() || !me->IsWithinDistInMap(victim, max_range) ||
        me->IsFriendlyTo(victim) || !me->canSeeOrDetect(victim) || victim->HasCrowdControlAura())
    {
        victim = NULL;
        SkyMistCore::NearestAttackableNoCCUnitInObjectRangeCheck u_check(me, me, max_range);
        SkyMistCore::UnitLastSearcher<SkyMistCore::NearestAttackableNoCCUnitInObjectRangeCheck> checker(me, victim, u_check);
        me->VisitNearbyObject(max_range, checker);
    }

    if (me->GetUInt32Value(UNIT_CREATED_BY_SPELL) == 3599) // Searing Totem
    {
        if (me->GetOwner() && me->GetOwner()->getVictim())
        {
            i_victimGuid = me->GetOwner()->getVictim()->GetGUID();
            victim = i_victimGuid ? ObjectAccessor::GetUnit(*me, i_victimGuid) : NULL;

            if (victim)
            {
                me->SetTarget(i_victimGuid);
                me->SetInCombatWith(victim);
            }
        }
    }

    // If have target
    if (victim)
    {
        // remember
        i_victimGuid = victim->GetGUID();

        // attack
        me->SetInFront(victim);                         // client change orientation by self
        me->CastSpell(victim, me->ToTotem()->GetSpell(), false);
    }
    else
        i_victimGuid = 0;
}

void TotemAI::AttackStart(Unit* /*victim*/)
{
    // Sentry totem sends ping on attack
    if (me->GetEntry() == SENTRY_TOTEM_ENTRY && me->GetOwner()->GetTypeId() == TYPEID_PLAYER)
    {        
        ObjectGuid guid = me->GetGUID();
        
        WorldPacket data(SMSG_MINIMAP_PING, 1 + 8 + 4 + 4);
        data.WriteBit(guid[6]);
        data.WriteBit(guid[3]);
        data.WriteBit(guid[7]);
        data.WriteBit(guid[5]);
        data.WriteBit(guid[4]);
        data.WriteBit(guid[2]);
        data.WriteBit(guid[1]);
        data.WriteBit(guid[0]);

        data.WriteByteSeq(guid[0]);
        data.WriteByteSeq(guid[5]);
        data << float(me->GetPositionY());
        data.WriteByteSeq(guid[7]);
        data.WriteByteSeq(guid[4]);
        data.WriteByteSeq(guid[3]);
        data.WriteByteSeq(guid[6]);
        data << float(me->GetPositionX());
        data.WriteByteSeq(guid[1]);
        data.WriteByteSeq(guid[2]);
        
        ((Player*)me->GetOwner())->GetSession()->SendPacket(&data);
    }
}
