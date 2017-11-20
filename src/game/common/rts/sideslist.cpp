/**
 * @file
 *
 * @author OmniBlade
 *
 * @brief Classes for managing sides.
 *
 * @copyright Thyme is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            2 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "sideslist.h"
#include "gamedebug.h"
#include "xfer.h"

#ifndef THYME_STNADALONE
SidesList *&g_theSidesList = Make_Global<SidesList *>(0x00A2BE3C);
#else
SidesList *g_theSidesList = nullptr;
#endif

BuildListInfo::BuildListInfo() :
    m_buildingName(),
    m_templateName(),
    m_location{0.0f, 0.0f, 0.0f},
    m_rallyPointOffset{0.0f, 0.0f},
    m_angle(0.0f),
    m_isInitiallyBuilt(false),
    m_numRebuilds(0),
    m_nextBuildList(nullptr),
    m_script(),
    m_health(100),
    m_whiner(true),
    m_repairable(false),
    m_sellable(true),
    m_autoBuild(true),
    m_renderObj(nullptr),
    m_shadowObj(nullptr),
    m_selected(false),
    m_objectID(OBJECT_UNK),
    m_objectTimestamp(0),
    m_underConstruction(false),
    m_unkbool3(false),
    m_unkint1(0),
    m_unkint2(0),
    m_unkbool4(false)
{
    memset(m_unkArray, 0, sizeof(m_unkArray));
}

BuildListInfo::~BuildListInfo()
{
    BuildListInfo *saved;
    for (BuildListInfo *next = m_nextBuildList; next != nullptr; next = saved) {
        saved = next->m_nextBuildList;
        Delete_Instance(next);
        next = saved;
    }
}

void BuildListInfo::Xfer_Snapshot(Xfer *xfer)
{
    uint8_t version = 2;
    xfer->xferVersion(&version, 2);
    xfer->xferAsciiString(&m_buildingName);
    xfer->xferAsciiString(&m_templateName);
    xfer->xferCoord3D(&m_location);
    xfer->xferCoord2D(&m_rallyPointOffset);
    xfer->xferReal(&m_angle);
    xfer->xferBool(&m_isInitiallyBuilt);
    xfer->xferUnsignedInt(&m_numRebuilds);
    xfer->xferAsciiString(&m_script);
    xfer->xferInt(&m_health);
    xfer->xferBool(&m_whiner);
    xfer->xferBool(&m_repairable);
    xfer->xferBool(&m_sellable);
    xfer->xferBool(&m_autoBuild);
    xfer->xferObjectID(&m_objectID);
    xfer->xferUnsignedInt(&m_objectTimestamp);
    xfer->xferBool(&m_underConstruction);
    xfer->xferUser(m_unkArray, sizeof(m_unkArray));
    xfer->xferBool(&m_unkbool3);
    xfer->xferInt(&m_unkint1);
    xfer->xferBool(&m_unkbool4);

    if (version >= 2) {
        xfer->xferInt(&m_unkint2);
    }
}

SidesInfo::SidesInfo(const SidesInfo &that) : m_buildList(nullptr), m_dict(0), m_scripts(nullptr)
{
    *this = that;
}

SidesInfo::~SidesInfo()
{
    Init();
}

void SidesInfo::Init(const Dict *dict)
{
    Delete_Instance(m_buildList);
    m_buildList = nullptr;
    Delete_Instance(m_scripts);
    m_scripts = nullptr;

    if (dict != nullptr) {
        m_dict = *dict;
    }
}

void SidesInfo::Add_To_BuildList(BuildListInfo *list, int pos)
{
    // TODO
}

int SidesInfo::Remove_From_BuildList(BuildListInfo *list)
{
    // TODO
    return 0;
}

void SidesInfo::Reorder_In_BuildList(BuildListInfo *list, int pos)
{
    Remove_From_BuildList(list);
    Add_To_BuildList(list, pos);
}

SidesInfo &SidesInfo::operator=(const SidesInfo &that)
{
#ifndef THYME_STANDALONE
    return Call_Method<SidesInfo &, SidesInfo, const SidesInfo &>(0x004D5C80, this, that);
#else
    if (this != &that) {
        Init(nullptr);
        m_dict = that.m_dict;

        //TODO Requires duplicate functions.
    }

    return *this;
#endif
}

SidesList::SidesList() : m_numSides(0), m_numSkirmishSides(0), m_teamRec(), m_skirmishTeamsRec() {}

void SidesList::Reset()
{
#ifndef THYME_STANDALONE
    Call_Method<void, SidesList>(0x004D6100, this);
#else
    // TODO Requires ScriptList ScriptGroup and BuildList
#endif
}

void SidesList::Xfer_Snapshot(Xfer *xfer)
{
    uint8_t version = 1;
    xfer->xferVersion(&version, 1);
    int32_t sides = m_numSides;
    xfer->xferInt(&sides);
    ASSERT_THROW(sides == m_numSides, 6);

    for (int i = 0; i < sides; ++i) {
        ASSERT_THROW(i < m_numSides, 0xDEAD0003);
        bool has_scripts = m_sides[i].Get_ScriptList() != nullptr;
        xfer->xferBool(&has_scripts);

        if (m_sides[i].Get_ScriptList() != nullptr) {
            ASSERT_THROW(has_scripts, 6);
            // Transfer script class.
            xfer->xferSnapshot(m_sides[i].Get_ScriptList()->Get_Scripts());
        }
    }
}
