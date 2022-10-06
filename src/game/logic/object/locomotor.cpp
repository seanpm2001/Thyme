/**
 * @file
 *
 * @author Jonathan Wilson
 *
 * @brief Locomotor
 *
 * @copyright Thyme is free software: you can redistribute it and/or
 *            modify it under the terms of the GNU General Public License
 *            as published by the Free Software Foundation, either version
 *            2 of the License, or (at your option) any later version.
 *            A full copy of the GNU General Public License can be found in
 *            LICENSE
 */
#include "locomotor.h"
#include "aipathfind.h"
#include "bodymodule.h"
#include "globaldata.h"
#include "object.h"
#include "physicsupdate.h"
#include "terrainlogic.h"

#ifndef GAME_DLL
LocomotorStore *g_theLocomotorStore = nullptr;
#endif

LocomotorTemplate::LocomotorTemplate() :
    m_maxSpeedDamaged(-1.0f),
    m_maxTurnRateDamaged(-1.0f),
    m_accelerationDamaged(-1.0f),
    m_liftDamaged(-1.0f),
    m_surfaces(0),
    m_maxSpeed(0.0f),
    m_maxTurnRate(0.0f),
    m_acceleration(0.0f),
    m_lift(0.0f),
    m_braking(99999.0f),
    m_minSpeed(0.0f),
    m_minTurnSpeed(99999.0f),
    m_behaviorZ(Z_NO_Z_MOTIVE_FORCE),
    m_appearance(LOCO_OTHER),
    m_groupMovementPriority(PRIORITY_MOVES_MIDDLE),
    m_preferredHeight(0.0f),
    m_preferredHeightDamping(1.0f),
    m_circlingRadius(0.0f),
    m_maxThrustAngle(0.0f),
    m_speedLimitZ(999999.0f),
    m_extra2DFriction(0.0f),
    m_accelPitchLimit(0.0f),
    m_decelPitchLimit(0.0f),
    m_bounceKick(0.0f),
    m_pitchStiffness(0.1f),
    m_rollStiffness(0.1f),
    m_pitchDamping(0.9f),
    m_rollDamping(0.9f),
    m_forwardVelCoef(0.0f),
    m_pitchInDirectionOfZVelFactor(0.0f),
    m_thrustRoll(0.0f),
    m_thrustWobbleRate(0.0f),
    m_thrustMinWobble(0.0f),
    m_thrustMaxWobble(0.0f),
    m_lateralVelCoef(0.0f),
    m_forwardAccelCoef(0.0f),
    m_lateralAccelCoef(0.0f),
    m_uniformAxialDamping(1.0f),
    m_turnPivotOffset(0.0f),
    m_apply2DFrictionWhenAirborne(false),
    m_downhillOnly(false),
    m_allowMotiveForceWhileAirborne(false),
    m_locomotorWorksWhenDead(false),
    m_airborneTargetingHeight(INT_MAX),
    m_stickToGround(false),
    m_canMoveBackwards(false),
    m_hasSuspension(false),
    m_wheelTurnAngle(0.0f),
    m_maximumWheelExtension(0.0f),
    m_maximumWheelCompression(0.0f),
    m_closeEnoughDist(1.0f),
    m_closeEnoughDist3D(false),
    m_slideIntoPlaceTime(0.0f),
    m_wanderWidthFactor(0.0f),
    m_wanderLengthFactor(1.0f),
    m_wanderAboutPointRadius(0.0f),
    m_rudderCorrectionDegree(0.0f),
    m_rudderCorrectionRate(0.0f),
    m_elevatorCorrectionDegree(0.0f),
    m_elevatorCorrectionRate(0.0f)
{
}

void LocomotorTemplate::Validate()
{
    if (m_maxSpeedDamaged < 0.0f) {
        m_maxSpeedDamaged = m_maxSpeed;
    }
    if (m_maxTurnRateDamaged < 0.0f) {
        m_maxTurnRateDamaged = m_maxTurnRate;
    }
    if (m_accelerationDamaged < 0.0f) {
        m_accelerationDamaged = m_acceleration;
    }
    if (m_liftDamaged < 0.0f) {
        m_liftDamaged = m_lift;
    }

    switch (m_appearance) {
        case LOCO_WINGS:
            if (m_minSpeed <= 0.0f) {
                captainslog_dbgassert(false, "WINGS should always have positive minSpeeds (otherwise they hover)");
                m_minSpeed = 0.01f;
            }

            if (m_minTurnSpeed <= 0.0f) {
                captainslog_dbgassert(false, "WINGS should always have positive minTurnSpeeds");
                m_minTurnSpeed = 0.01f;
            }

            break;
        case LOCO_THRUST:
            captainslog_relassert(
                m_behaviorZ == LocomotorBehaviorZ::Z_NO_Z_MOTIVE_FORCE && m_lift == 0.0f && m_liftDamaged == 0.0f,
                0xDEAD0006,
                "THRUST locos may not use ZAxisBehaviour or lift!");

            if (m_maxSpeed <= 0.0f) {
                captainslog_debug("THRUST locos may not have zero m_maxSpeed; healing...");
                m_maxSpeed = 0.01f;
            }

            if (m_maxSpeedDamaged <= 0.0f) {
                captainslog_debug("THRUST locos may not have zero m_maxSpeedDamaged; healing...");
                m_maxSpeedDamaged = 0.01f;
            }

            if (m_minSpeed <= 0.0f) {
                captainslog_debug("THRUST locos may not have zero m_minSpeed; healing...");
                m_minSpeed = 0.01f;
            }

            break;
    }
}

const char *g_theLocomotorSurfaceTypeNames[] = {
    "GROUND",
    "WATER",
    "CLIFF",
    "AIR",
    "RUBBLE",
    nullptr,
};

const char *g_theLocomotorBehaviorZNames[] = {
    "NO_Z_MOTIVE_FORCE",
    "SEA_LEVEL",
    "SURFACE_RELATIVE_HEIGHT",
    "ABSOLUTE_HEIGHT",
    "FIXED_SURFACE_RELATIVE_HEIGHT",
    "FIXED_ABSOLUTE_HEIGHT",
    "FIXED_RELATIVE_TO_GROUND_AND_BUILDINGS",
    "RELATIVE_TO_HIGHEST_LAYER",
    nullptr,
};

const char *g_theLocomotorAppearanceNames[] = {
    "TWO_LEGS",
    "FOUR_WHEELS",
    "TREADS",
    "HOVER",
    "THRUST",
    "WINGS",
    "CLIMBER",
    "OTHER",
    "MOTORCYCLE",
    nullptr,
};

const char *g_theLocomotorPriorityNames[] = {
    "MOVES_BACK",
    "MOVES_MIDDLE",
    "MOVES_FRONT",
    nullptr,
};

void Parse_Friction_Per_Sec(INI *ini, void *, void *store, const void *)
{
    *static_cast<float *>(store) = ini->Scan_Real(ini->Get_Next_Token()) * 1.0f / 30.0f;
}

// clang-format off
FieldParse LocomotorTemplate::s_fieldParseTable[] = {
    { "Surfaces", &INI::Parse_Bitstring32, g_theLocomotorSurfaceTypeNames, offsetof(LocomotorTemplate, m_surfaces) },
    { "Speed", &INI::Parse_Velocity_Real, nullptr, offsetof(LocomotorTemplate, m_maxSpeed) },
    { "SpeedDamaged", &INI::Parse_Velocity_Real, nullptr, offsetof(LocomotorTemplate, m_maxSpeedDamaged) },
    { "MinSpeed", &INI::Parse_Velocity_Real, nullptr, offsetof(LocomotorTemplate, m_minSpeed) },
    { "TurnRate", &INI::Parse_Angular_Velocity_Real, nullptr, offsetof(LocomotorTemplate, m_maxTurnRate) },
    { "TurnRateDamaged", &INI::Parse_Angular_Velocity_Real, nullptr, offsetof(LocomotorTemplate, m_maxTurnRateDamaged) },
    { "Acceleration", &INI::Parse_Acceleration_Real, nullptr, offsetof(LocomotorTemplate, m_acceleration) },
    { "AccelerationDamaged", &INI::Parse_Acceleration_Real, nullptr, offsetof(LocomotorTemplate, m_accelerationDamaged) },
    { "Lift", &INI::Parse_Acceleration_Real, nullptr, offsetof(LocomotorTemplate, m_lift) },
    { "LiftDamaged", &INI::Parse_Acceleration_Real, nullptr, offsetof(LocomotorTemplate, m_liftDamaged) },
    { "Braking", &INI::Parse_Acceleration_Real, nullptr, offsetof(LocomotorTemplate, m_braking) },
    { "MinTurnSpeed", &INI::Parse_Velocity_Real, nullptr, offsetof(LocomotorTemplate, m_minTurnSpeed) },
    { "PreferredHeight", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_preferredHeight) },
    { "PreferredHeightDamping", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_preferredHeightDamping) },
    { "CirclingRadius", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_circlingRadius) },
    { "SpeedLimitZ", &INI::Parse_Velocity_Real, nullptr, offsetof(LocomotorTemplate, m_speedLimitZ) },
    { "Extra2DFriction", &Parse_Friction_Per_Sec, nullptr, offsetof(LocomotorTemplate, m_extra2DFriction) },
    { "MaxThrustAngle", &INI::Parse_Angle_Real, nullptr, offsetof(LocomotorTemplate, m_maxThrustAngle) },
    { "ZAxisBehavior", &INI::Parse_Index_List, g_theLocomotorBehaviorZNames, offsetof(LocomotorTemplate, m_behaviorZ) },
    { "Appearance", &INI::Parse_Index_List, g_theLocomotorAppearanceNames, offsetof(LocomotorTemplate, m_appearance) },
    { "GroupMovementPriority", &INI::Parse_Index_List, g_theLocomotorPriorityNames, offsetof(LocomotorTemplate, m_groupMovementPriority) },
    { "AccelerationPitchLimit", &INI::Parse_Angle_Real, nullptr, offsetof(LocomotorTemplate, m_accelPitchLimit) },
    { "DecelerationPitchLimit", &INI::Parse_Angle_Real, nullptr, offsetof(LocomotorTemplate, m_decelPitchLimit) },
    { "BounceAmount", &INI::Parse_Angular_Velocity_Real, nullptr, offsetof(LocomotorTemplate, m_bounceKick) },
    { "PitchStiffness", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_pitchStiffness) },
    { "RollStiffness", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_rollStiffness) },
    { "PitchDamping", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_pitchDamping) },
    { "RollDamping", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_rollDamping) },
    { "PitchInDirectionOfZVelFactor", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_pitchInDirectionOfZVelFactor) },
    { "ThrustRoll", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_thrustRoll) },
    { "ThrustWobbleRate", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_thrustWobbleRate) },
    { "ThrustMinWobble", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_thrustMinWobble) },
    { "ThrustMaxWobble", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_thrustMaxWobble) },
    { "ForwardVelocityPitchFactor", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_forwardVelCoef) },
    { "LateralVelocityRollFactor", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_lateralVelCoef) },
    { "ForwardAccelerationPitchFactor", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_forwardAccelCoef) },
    { "LateralAccelerationRollFactor", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_lateralAccelCoef) },
    { "UniformAxialDamping", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_uniformAxialDamping) },
    { "TurnPivotOffset", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_turnPivotOffset) },
    { "AirborneTargetingHeight", &INI::Parse_Int, nullptr, offsetof(LocomotorTemplate, m_airborneTargetingHeight) },
    { "CloseEnoughDist", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_closeEnoughDist) },
    { "CloseEnoughDist3D", &INI::Parse_Bool, nullptr, offsetof(LocomotorTemplate, m_closeEnoughDist3D) },
    { "SlideIntoPlaceTime", &INI::Parse_Duration_Real, nullptr, offsetof(LocomotorTemplate, m_slideIntoPlaceTime) },
    { "LocomotorWorksWhenDead", &INI::Parse_Bool, nullptr, offsetof(LocomotorTemplate, m_locomotorWorksWhenDead) },
    { "AllowAirborneMotiveForce", &INI::Parse_Bool, nullptr, offsetof(LocomotorTemplate, m_allowMotiveForceWhileAirborne) },
    { "Apply2DFrictionWhenAirborne", &INI::Parse_Bool, nullptr, offsetof(LocomotorTemplate, m_apply2DFrictionWhenAirborne) },
    { "DownhillOnly", &INI::Parse_Bool, nullptr, offsetof(LocomotorTemplate, m_downhillOnly) },
    { "StickToGround", &INI::Parse_Bool, nullptr, offsetof(LocomotorTemplate, m_stickToGround) },
    { "CanMoveBackwards", &INI::Parse_Bool, nullptr, offsetof(LocomotorTemplate, m_canMoveBackwards) },
    { "HasSuspension", &INI::Parse_Bool, nullptr, offsetof(LocomotorTemplate, m_hasSuspension) },
    { "MaximumWheelExtension", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_maximumWheelExtension) },
    { "MaximumWheelCompression", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_maximumWheelCompression) },
    { "FrontWheelTurnAngle", &INI::Parse_Angle_Real, nullptr, offsetof(LocomotorTemplate, m_wheelTurnAngle) },
    { "WanderWidthFactor", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_wanderWidthFactor) },
    { "WanderLengthFactor", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_wanderLengthFactor) },
    { "WanderAboutPointRadius", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_wanderAboutPointRadius) },
    { "RudderCorrectionDegree", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_rudderCorrectionDegree) },
    { "RudderCorrectionRate", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_rudderCorrectionRate) },
    { "ElevatorCorrectionDegree", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_elevatorCorrectionDegree) },
    { "ElevatorCorrectionRate", &INI::Parse_Real, nullptr, offsetof(LocomotorTemplate, m_elevatorCorrectionRate) },
    { nullptr, nullptr, nullptr, 0 },
};
// clang-format on

LocomotorStore::~LocomotorStore()
{
    for (auto it = m_locomotorTemplates.begin(); it != m_locomotorTemplates.end(); it++) {
        it->second->Delete_Instance();
    }

    m_locomotorTemplates.clear();
}

LocomotorTemplate *LocomotorStore::Find_Locomotor_Template(NameKeyType namekey)
{
    if (namekey == NAMEKEY_INVALID) {
        return nullptr;
    }

    auto t = m_locomotorTemplates.find(namekey);

    if (t != m_locomotorTemplates.end()) {
        return (*t).second;
    } else {
        return nullptr;
    }
}

const LocomotorTemplate *LocomotorStore::Find_Locomotor_Template(NameKeyType namekey) const
{
    if (namekey == NAMEKEY_INVALID) {
        return nullptr;
    }

    auto t = m_locomotorTemplates.find(namekey);

    if (t != m_locomotorTemplates.end()) {
        return (*t).second;
    } else {
        return nullptr;
    }
}

void LocomotorStore::Reset()
{
    auto it = m_locomotorTemplates.begin();

    for (;;) {
        if (!(it != m_locomotorTemplates.end())) {
            break;
        }

        if (it->second->Delete_Overrides() == nullptr) {
            m_locomotorTemplates.erase(it);
        } else {
            ++it;
        }
    }
}

Locomotor *LocomotorStore::New_Locomotor(const LocomotorTemplate *tmpl)
{
    return new Locomotor(tmpl);
}

LocomotorTemplate *LocomotorStore::New_Override(LocomotorTemplate *tmpl)
{
    if (tmpl == nullptr) {
        return nullptr;
    }

    LocomotorTemplate *t = new LocomotorTemplate(*tmpl);
    tmpl->Set_Next(t);
    t->Set_Is_Allocated();
    return t;
}

void LocomotorStore::Parse_Locomotor_Template_Definition(INI *ini)
{
    if (g_theLocomotorStore == nullptr) {
        throw CODE_06;
    }

    bool found = false;
    const char *name = ini->Get_Next_Token();
    NameKeyType key = g_theNameKeyGenerator->Name_To_Key(name);

    LocomotorTemplate *t = g_theLocomotorStore->Find_Locomotor_Template(key);
    if (t != nullptr) {
        if (ini->Get_Load_Type() == INILoadType::INI_LOAD_CREATE_OVERRIDES) {
            t = g_theLocomotorStore->New_Override(static_cast<LocomotorTemplate *>(t->Friend_Get_Final_Override()));
        }

        found = true;
    } else {
        t = NEW_POOL_OBJ(LocomotorTemplate);

        if (ini->Get_Load_Type() == INILoadType::INI_LOAD_CREATE_OVERRIDES) {
            t->Set_Is_Allocated();
        }
    }

    t->Set_Name(name);
    ini->Init_From_INI(t, LocomotorTemplate::Get_Field_Parse());
    t->Validate();

    if (!found) {
        g_theLocomotorStore->m_locomotorTemplates[key] = t;
    }
}

Locomotor::Locomotor(const LocomotorTemplate *tmpl) :
    m_brakingFactor(1.0f),
    m_maxLift(99999.0f),
    m_maxSpeed(99999.0f),
    m_maxAccel(99999.0f),
    m_maxBraking(99999.0f),
    m_maxTurnRate(99999.0f),
    m_flags(0)
{
    m_template = tmpl;
    m_closeEnoughDist = m_template->m_closeEnoughDist;
    Set_Flag(CLOSE_ENOUGH_DIST_3D, m_template->m_closeEnoughDist3D);
    m_preferredHeight = m_template->m_preferredHeight;
    m_preferredHeightDamping = m_template->m_preferredHeightDamping;
    m_wanderAngle = Get_Logic_Random_Value_Real(DEG_TO_RADF(-30), DEG_TO_RADF(30));
    m_wanderLength = Get_Logic_Random_Value_Real(0.80000001f, 1.2f);
    Set_Flag(WANDER_DIRECTION, (Get_Logic_Random_Value(0, 1) != 0));
    m_moveFrame = g_theGameLogic->Get_Frame() + 2.5f * 30.0f;
}

Locomotor::Locomotor(const Locomotor &that) :
    m_template(that.m_template),
    m_brakingFactor(that.m_brakingFactor),
    m_maxLift(that.m_maxLift),
    m_maxSpeed(that.m_maxSpeed),
    m_maxAccel(that.m_maxAccel),
    m_maxBraking(that.m_maxBraking),
    m_maxTurnRate(that.m_maxTurnRate),
    m_flags(that.m_flags),
    m_closeEnoughDist(that.m_closeEnoughDist),
    m_preferredHeight(that.m_preferredHeight),
    m_preferredHeightDamping(that.m_preferredHeightDamping),
    m_wanderAngle(that.m_wanderAngle),
    m_wanderLength(that.m_wanderLength)
{
    m_maintainPos.Zero();
}

Locomotor &Locomotor::operator=(const Locomotor &that)
{
    if (this != &that) {
        m_template = that.m_template;
        m_brakingFactor = that.m_brakingFactor;
        m_maxLift = that.m_maxLift;
        m_maxSpeed = that.m_maxSpeed;
        m_maxAccel = that.m_maxAccel;
        m_maxBraking = that.m_maxBraking;
        m_maxTurnRate = that.m_maxTurnRate;
        m_flags = that.m_flags;
        m_closeEnoughDist = that.m_closeEnoughDist;
        m_preferredHeight = that.m_preferredHeight;
        m_preferredHeightDamping = that.m_preferredHeightDamping;
    }

    return *this;
}

void Locomotor::Xfer_Snapshot(Xfer *xfer)
{
    uint8_t version = 2;
    xfer->xferVersion(&version, 2);

    if (version >= 2) {
        xfer->xferUnsignedInt(&m_moveFrame);
    }

    xfer->xferCoord3D(&m_maintainPos);
    xfer->xferReal(&m_brakingFactor);
    xfer->xferReal(&m_maxLift);
    xfer->xferReal(&m_maxSpeed);
    xfer->xferReal(&m_maxAccel);
    xfer->xferReal(&m_maxBraking);
    xfer->xferReal(&m_maxTurnRate);
    xfer->xferReal(&m_closeEnoughDist);
    xfer->xferUnsignedInt(&m_flags);
    xfer->xferReal(&m_preferredHeight);
    xfer->xferReal(&m_preferredHeightDamping);
    xfer->xferReal(&m_wanderAngle);
    xfer->xferReal(&m_wanderLength);
}

float Locomotor::Get_Max_Speed_For_Condition(BodyDamageType condition) const
{
    float max_speed;

    if (condition < g_theWriteableGlobalData->m_movementPenaltyDamageState) {
        max_speed = m_template->m_maxSpeed;
    } else {
        max_speed = m_template->m_maxSpeedDamaged;
    }

    if (max_speed > m_maxSpeed) {
        return m_maxSpeed;
    }

    return max_speed;
}

float Locomotor::Get_Max_Turn_Rate(BodyDamageType condition) const
{
    float max_turn;

    if (condition < g_theWriteableGlobalData->m_movementPenaltyDamageState) {
        max_turn = m_template->m_maxTurnRate;
    } else {
        max_turn = m_template->m_maxTurnRateDamaged;
    }

    if (max_turn > m_maxTurnRate) {
        max_turn = m_maxTurnRate;
    }

    if (Get_Flag(ULTRA_ACCURATE)) {
        return max_turn * 2.0f;
    }

    return max_turn;
}

float Locomotor::Get_Max_Acceleration(BodyDamageType condition) const
{
    float max_accel;

    if (condition < g_theWriteableGlobalData->m_movementPenaltyDamageState) {
        max_accel = m_template->m_acceleration;
    } else {
        max_accel = m_template->m_accelerationDamaged;
    }

    if (max_accel > m_maxAccel) {
        return m_maxAccel;
    }

    return max_accel;
}

float Locomotor::Get_Max_Lift(BodyDamageType condition) const
{
    float max_lift;

    if (condition < g_theWriteableGlobalData->m_movementPenaltyDamageState) {
        max_lift = m_template->m_lift;
    } else {
        max_lift = m_template->m_liftDamaged;
    }

    if (max_lift > m_maxLift) {
        return m_maxLift;
    }

    return max_lift;
}

float Locomotor::Get_Surface_Ht_At_Pt(float x, float y)
{
    float f = 0.0f;
    float waterz;
    float groundz;

    if (g_theTerrainLogic->Is_Underwater(x, y, &waterz, &groundz)) {
        return f + waterz;
    } else {
        return f + groundz;
    }
}

float Locomotor::Get_Braking() const
{
    float braking = m_template->m_braking;

    if (braking > m_maxBraking) {
        return m_maxBraking;
    }

    return braking;
}

void Locomotor::Loco_Update_Move_Towards_Position(
    Object *obj, const Coord3D &goal_pos, float on_path_dist_to_goal, float desired_speed, bool *blocked)
{
    Set_Flag(MAINTAIN_POS_IS_VALID, false);
    float max_speed = Get_Max_Speed_For_Condition(obj->Get_Body_Module()->Get_Damage_State());

    if (desired_speed > max_speed) {
        desired_speed = max_speed;
    }

    float speed = max_speed / Get_Braking() * max_speed / 2.0f;

    if (on_path_dist_to_goal > 10.0f && on_path_dist_to_goal > speed) {
        Set_Flag(IS_BRAKING, false);
        m_brakingFactor = 1.0f;
    }

    PhysicsBehavior *physics = obj->Get_Physics();

    if (physics == nullptr) {
        captainslog_dbgassert(false, "you can only apply Locomotors to objects with Physics");
    } else if (!physics->Get_Stunned()) {
        if ((m_template->m_surfaces & LOCOMOTOR_SURFACE_AIR) != 0
            || g_theAI->Get_Pathfinder()->Valid_Movement_Terrain(obj->Get_Layer(), this, obj->Get_Position())
            || Get_Flag(ALLOW_INVALID_POSITION) || !Fix_Invalid_Position(obj, physics)) {
            float x = goal_pos.x - obj->Get_Position()->x;
            float y = goal_pos.y - obj->Get_Position()->y;
            float z = goal_pos.z - obj->Get_Position()->z;
            float path_dist = GameMath::Sqrt(x * x + y * y);

            if (path_dist > on_path_dist_to_goal) {
                if (!obj->Is_KindOf(KINDOF_PROJECTILE) && 2.0f * on_path_dist_to_goal < path_dist) {
                    Set_Flag(IS_BRAKING, true);
                }

                on_path_dist_to_goal = path_dist;
            }

            bool is_airbone = false;
            Coord3D pos = *obj->Get_Position();
            float height = pos.z - g_theTerrainLogic->Get_Layer_Height(pos.x, pos.y, obj->Get_Layer(), nullptr, true);

            if (obj->Get_Status_Bits().Test(OBJECT_STATUS_DECK_HEIGHT_OFFSET)) {
                height -= obj->Get_Carrier_Deck_Height();
            }

            if (-9.0f * g_theWriteableGlobalData->m_gravity < height) {
                is_airbone = true;
            }

            Coord3D force3d;
            force3d.Zero();
            physics->Apply_Motive_Force(&force3d);

            if (*blocked) {
                if (physics->Get_Velocity_Magnitude() < desired_speed) {
                    *blocked = false;
                }

                if (is_airbone && ((m_template->m_surfaces & LOCOMOTOR_SURFACE_AIR) != 0)) {
                    *blocked = false;
                }
            }

            if (*blocked) {
                physics->Scrub_Velocity_2D(desired_speed);
                float turn = Get_Max_Turn_Rate(obj->Get_Body_Module()->Get_Damage_State());

                if (m_template->m_wanderWidthFactor == 0.0f) {
                    *blocked = Rotate_Obj_Around_Loco_Pivot(obj, goal_pos, turn, nullptr) != TURN_NONE;
                }

                Handle_Behavior_Z(obj, physics, goal_pos);
            } else {
                if (m_template->m_appearance == LOCO_WINGS) {
                    Set_Flag(IS_BRAKING, false);
                }

                bool braking = obj->Get_Status_Bits().Test(OBJECT_STATUS_IS_BRAKING);
                physics->Set_Turning(TURN_NONE);

                if (Get_Allow_Motive_Force_While_Airborne() || !is_airbone) {
                    switch (m_template->m_appearance) {
                        case LOCO_LEGS_TWO:
                            Move_Towards_Position_Legs(obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
                            break;
                        case LOCO_WHEELS_FOUR:
                        case LOCO_MOTORCYCLE:
                            Move_Towards_Position_Wheels(obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
                            break;
                        case LOCO_TREADS:
                            Move_Towards_Position_Treads(obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
                            break;
                        case LOCO_HOVER:
                            Move_Towards_Position_Hover(obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
                            break;
                        case LOCO_THRUST:
                            Move_Towards_Position_Thrust(obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
                            break;
                        case LOCO_WINGS:
                            Move_Towards_Position_Wings(obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
                            break;
                        case LOCO_CLIMBER:
                            Move_Towards_Position_Climb(obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
                            break;
                        default:
                            Move_Towards_Position_Other(obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
                            break;
                    }
                }

                Handle_Behavior_Z(obj, physics, goal_pos);
                obj->Set_Status(
                    BitFlags<OBJECT_STATUS_COUNT>(BitFlags<OBJECT_STATUS_COUNT>::kInit, OBJECT_STATUS_IS_BRAKING),
                    Get_Flag(IS_BRAKING));

                if (braking) {
                    Coord3D new_pos = *obj->Get_Position();

                    if (obj->Is_KindOf(KINDOF_PROJECTILE)) {
                        obj->Set_Status(
                            BitFlags<OBJECT_STATUS_COUNT>(BitFlags<OBJECT_STATUS_COUNT>::kInit, OBJECT_STATUS_IS_BRAKING),
                            true);
                        path_dist = GameMath::Sqrt(x * x + y * y + z * z);
                        float magnitude = physics->Get_Velocity_Magnitude();

                        if (magnitude < 1.0f / 30.0f) {
                            magnitude = 1.0f / 30.0f;
                        }

                        if (magnitude > path_dist) {
                            magnitude = path_dist;
                        }

                        if (path_dist > 0.001f) {
                            path_dist = 1.0f / path_dist;
                            x = x * path_dist;
                            y = y * path_dist;
                            z = z * path_dist;
                            new_pos.x = x * magnitude + new_pos.x;
                            new_pos.y = y * magnitude + new_pos.y;
                            new_pos.z = z * magnitude + new_pos.z;
                        }
                    } else if (path_dist > 0.001f) {
                        float cur_speed = GameMath::Fabs(physics->Get_Forward_Speed_2D());

                        if (cur_speed < 1.0f / 30.0f) {
                            cur_speed = 1.0f / 30.0f;
                        }

                        if (cur_speed > path_dist) {
                            cur_speed = path_dist;
                        }

                        path_dist = 1.0f / path_dist;
                        x = x * path_dist;
                        y = y * path_dist;
                        new_pos.x = x * cur_speed + new_pos.x;
                        new_pos.y = y * cur_speed + new_pos.y;
                    }

                    obj->Set_Position(&new_pos);
                }
            }
        }
    }
}

float Normalize_Angle(float angle1, float angle2)
{
    return Normalize_Angle(angle1 - angle2);
}

float Calc_Slow_Down_Dist(float cur_speed, float desired_speed, float max_breaking)
{
    float speed = cur_speed - desired_speed;

    if (speed <= 0.0f) {
        return 0.0f;
    }

    float distance = GameMath::Square(speed) / GameMath::Fabs(max_breaking) * 0.5f;
    return distance * 1.05f;
}

void Locomotor::Loco_Update_Move_Towards_Angle(Object *obj, float goal_angle)
{
    Set_Flag(MAINTAIN_POS_IS_VALID, false);

    if (obj != nullptr && *m_template != nullptr) {
        PhysicsBehavior *physics = obj->Get_Physics();

        if (physics == nullptr) {
            captainslog_dbgassert(false, "you can only apply Locomotors to objects with Physics");
        } else {
            if (!physics->Get_Stunned()) {
                float min_speed = Get_Min_Speed();

                if (min_speed > 0.0f) {
                    Coord3D pos = *obj->Get_Position();
                    float c = GameMath::Cos(goal_angle);
                    pos.x = c * min_speed + c * min_speed + pos.x;
                    float s = GameMath::Sin(goal_angle);
                    pos.y = s * min_speed + s * min_speed + pos.y;
                    bool blocked = false;
                    Loco_Update_Move_Towards_Position(obj, pos, 99999.0f, min_speed, &blocked);
                } else {
                    captainslog_dbgassert(m_template->m_appearance != LOCO_THRUST, "THRUST should always have minspeeds!");
                    Coord3D pos = *obj->Get_Position();
                    pos.x = GameMath::Cos(goal_angle) * 1000.0f + pos.x;
                    pos.y = GameMath::Sin(goal_angle) * 1000.0f + pos.y;
                    physics->Set_Turning(Rotate_Towards_Position(obj, pos, nullptr));
                    Handle_Behavior_Z(obj, physics, *obj->Get_Position());
                }
            }
        }
    }
}

PhysicsTurningType Locomotor::Rotate_Towards_Position(Object *obj, const Coord3D &position, float *angle)
{
    return Rotate_Obj_Around_Loco_Pivot(obj, position, Get_Max_Turn_Rate(obj->Get_Body_Module()->Get_Damage_State()), angle);
}

void Locomotor::Move_Towards_Position_Legs(
    Object *obj, PhysicsBehavior *physics, const Coord3D &goal_pos, float on_path_dist_to_goal, float desired_speed)
{
    if (!Get_Downhill_Only() || obj->Get_Position()->z >= goal_pos.z) {
        float max_accel = Get_Max_Acceleration(obj->Get_Body_Module()->Get_Damage_State());
        float max_speed = Get_Max_Speed_For_Condition(obj->Get_Body_Module()->Get_Damage_State());

        if (desired_speed > max_speed) {
            desired_speed = max_speed;
        }

        float cur_speed = physics->Get_Forward_Speed_2D();
        float orientation = obj->Get_Orientation();
        float goal_angle = GameMath::Atan2(goal_pos.y - obj->Get_Position()->y, goal_pos.x - obj->Get_Position()->x);

        if (m_template->m_wanderWidthFactor != 0.0f) {
            float wander_angle = DEG_TO_RADF(22.5f) * m_template->m_wanderWidthFactor;

            if (Get_Flag(WANDER_DIRECTION)) {
                m_wanderAngle = m_wanderAngle + cur_speed * m_wanderLength;

                if (m_wanderAngle > wander_angle) {
                    Set_Flag(WANDER_DIRECTION, false);
                }
            } else {
                m_wanderAngle = m_wanderAngle - cur_speed * m_wanderLength;

                if (-wander_angle > m_wanderAngle) {
                    Set_Flag(WANDER_DIRECTION, true);
                }
            }

            goal_angle = Normalize_Angle(goal_angle + m_wanderAngle);
        }

        float turn_angle = Normalize_Angle(goal_angle, orientation);
        Loco_Update_Move_Towards_Angle(obj, goal_angle);

        float acute_factor = GameMath::Fabs(turn_angle) / DEG_TO_RADF(45.0f);

        if (acute_factor > 1.0f) {
            acute_factor = 1.0f;
        }

        float acute_speed = (1.0f - acute_factor) * desired_speed;
        float brake_dist = Calc_Slow_Down_Dist(cur_speed, m_template->m_minSpeed, Get_Braking());

        if (on_path_dist_to_goal < brake_dist && !Get_Flag(NO_SLOW_DOWN_AS_APPROACHING_DEST)) {
            acute_speed = m_template->m_minSpeed;
        }

        float accute_accel = acute_speed - cur_speed;

        if (accute_accel != 0.0f) {
            float mass = physics->Get_Mass();
            float accel;

            if (accute_accel > 0.0f) {
                accel = max_accel;
            } else {
                accel = -Get_Braking();
            }

            float force = mass * accel;
            float accute_force = mass * accute_accel;

            if (GameMath::Fabs(accute_force) < GameMath::Fabs(force)) {
                force = accute_force;
            }

            Coord3D force3d;
            const Coord3D *dir = obj->Get_Unit_Dir_Vector2D();
            force3d.x = force * dir->x;
            force3d.y = force * dir->y;
            force3d.z = 0.0f;
            physics->Apply_Motive_Force(&force3d);
        }
    }
}

void Locomotor::Move_Towards_Position_Wheels(
    Object *obj, PhysicsBehavior *physics, const Coord3D &goal_pos, float on_path_dist_to_goal, float desired_speed)
{
    BodyDamageType damage = obj->Get_Body_Module()->Get_Damage_State();
    float max_speed = Get_Max_Speed_For_Condition(damage);
    float max_turn = Get_Max_Turn_Rate(damage);
    float max_accel = Get_Max_Acceleration(damage);

    if (desired_speed > max_speed) {
        desired_speed = max_speed;
    }

    float turn_speed = m_template->m_minTurnSpeed;
    float orientation = obj->Get_Orientation();
    float goal_angle = GameMath::Atan2(goal_pos.y - obj->Get_Position()->y, goal_pos.x - obj->Get_Position()->x);

    float turn_angle = Normalize_Angle(goal_angle, orientation);
    bool backwards = false;

    if (max_speed / 4.0f > turn_speed) {
        turn_speed = max_speed / 4.0f;
    }

    float cur_speed = physics->Get_Forward_Speed_2D();
    bool b = false;

    if (cur_speed == 0.0f) {
        Set_Flag(MOVING_BACKWARDS, false);

        if (m_template->m_canMoveBackwards && GameMath::Fabs(turn_angle) > DEG_TO_RADF(90.0f)) {
            Set_Flag(MOVING_BACKWARDS, true);
            Set_Flag(FLAG_8, obj->Get_Geometry_Info().Get_Major_Radius() * 5.0f < on_path_dist_to_goal);
        }
    }

    if (Get_Flag(MOVING_BACKWARDS)) {
        if (GameMath::Fabs(turn_angle) < DEG_TO_RADF(90.0f)) {
            backwards = false;
            Set_Flag(MOVING_BACKWARDS, false);
        } else {
            backwards = true;
            Set_Flag(FLAG_8, obj->Get_Geometry_Info().Get_Major_Radius() * 5.0f < on_path_dist_to_goal);
            b = Get_Flag(FLAG_8);

            if (!b) {
                goal_angle = Normalize_Angle(goal_angle, DEG_TO_RADF(180.0f));
                turn_angle = Normalize_Angle(goal_angle, orientation);
            }
        }
    }

    if (GameMath::Fabs(turn_angle) > DEG_TO_RADF(9.0f) && desired_speed > turn_speed) {
        desired_speed = turn_speed;
    }

    if (backwards) {
        cur_speed = -cur_speed;
    }

    float speed1 = cur_speed / Get_Braking() + 1.0f;
    float speed2 = cur_speed / 1.5f * speed1 + cur_speed;
    float speed3 = speed2;

    if (speed2 < 10.0f) {
        speed3 = 10.0f;
    }

    float speed4;
    float orientation2;
    float speed5;
    float speed6;
    float c;
    float s;
    Coord3D pos;
    Coord3D pos2;

    if (GameMath::Fabs(turn_angle) <= DEG_TO_RADF(15.0f)) {
        goto l1;
    }

    speed4 = (desired_speed + cur_speed) * 15.0f / 2.0f;
    orientation2 = obj->Get_Orientation();
    speed5 = (desired_speed + cur_speed) / 2.0f / turn_speed;

    if (speed5 > 1.0f) {
        speed5 = 1.0f;
    }

    speed6 = 15.0f * speed5 * max_turn / 4.0f;
    orientation2 = turn_angle >= 0.0f ? orientation2 + speed6 : orientation2 - speed6;
    c = GameMath::Cos(orientation2) * speed4;
    s = GameMath::Sin(orientation2) * speed4;

    pos.x = c + obj->Get_Position()->x;
    pos.y = s + obj->Get_Position()->y;
    pos.z = obj->Get_Position()->z;

    pos2.x = c / 2.0f + obj->Get_Position()->x;
    pos2.y = s / 2.0f + obj->Get_Position()->y;
    pos2.z = obj->Get_Position()->z;

    if (!g_theAI->Get_Pathfinder()->Valid_Movement_Terrain(obj->Get_Layer(), this, &pos2)) {
        goto l2;
    }

    if (!g_theAI->Get_Pathfinder()->Valid_Movement_Terrain(obj->Get_Layer(), this, &pos)) {
    l2:
        physics->Set_Turning(Rotate_Towards_Position(obj, goal_pos, nullptr));
        Coord3D force3d;
        force3d.Zero();
        physics->Apply_Motive_Force(&force3d);
    } else {
    l1:
        if (on_path_dist_to_goal < speed3 && !Get_Flag(IS_BRAKING) && !Get_Flag(NO_SLOW_DOWN_AS_APPROACHING_DEST)) {
            Set_Flag(IS_BRAKING, true);
            m_brakingFactor = 1.1f;
        }

        if (on_path_dist_to_goal > 10.0f && speed2 + speed2 < on_path_dist_to_goal) {
            Set_Flag(IS_BRAKING, false);
        }

        if (on_path_dist_to_goal > 40.0f) {
            m_moveFrame = g_theGameLogic->Get_Frame() + 2.5f * 30.0f;
        } else if (m_moveFrame < g_theGameLogic->Get_Frame()) {
            Set_Flag(IS_BRAKING, true);
        }

        if (Get_Flag(IS_BRAKING)) {
            m_brakingFactor = speed2 / on_path_dist_to_goal;
            m_brakingFactor = m_brakingFactor * m_brakingFactor;

            if (m_brakingFactor > 5.0f) {
                m_brakingFactor = 5.0;
            }

            m_brakingFactor = 1.0;

            if (speed2 > on_path_dist_to_goal) {
                desired_speed = cur_speed - Get_Braking();

                if (desired_speed < 0.0f) {
                    desired_speed = 0.0f;
                }
            } else if (on_path_dist_to_goal * 0.75f < speed2) {
                desired_speed = cur_speed - Get_Braking() / 2.0f;

                if (desired_speed < 0.0f) {
                    desired_speed = 0.0f;
                }
            } else {
                desired_speed = cur_speed;
            }
        }

        float speed7 = cur_speed / turn_speed;

        if (speed7 < 0.0f) {
            speed7 = -speed7;
        }

        if (speed7 > 1.0f) {
            speed7 = 1.0f;
        }

        float speed8 = speed7 * max_turn;

        if (backwards && !b) {
            Coord3D pos3 = *obj->Get_Position();
            pos3.x = pos3.x - (goal_pos.x - obj->Get_Position()->x);
            pos3.y = pos3.y - (goal_pos.y - obj->Get_Position()->x);
            physics->Set_Turning(Rotate_Obj_Around_Loco_Pivot(obj, pos3, speed8, nullptr));
        } else {
            physics->Set_Turning(Rotate_Obj_Around_Loco_Pivot(obj, goal_pos, speed8, nullptr));
        }

        float reduced_accel = desired_speed - cur_speed;

        if (backwards) {
            reduced_accel = cur_speed - desired_speed;
        }

        if (reduced_accel != 0.0f) {
            float accel;
            float mass = physics->Get_Mass();

            if (backwards) {
                if (reduced_accel < 0.0f) {
                    accel = -max_accel;
                } else {
                    accel = Get_Braking() * m_brakingFactor;
                }
            } else if (reduced_accel > 0.0f) {
                accel = max_accel;
            } else {
                accel = Get_Braking() * -m_brakingFactor;
            }

            float force = mass * accel;
            float reduced_force = mass * reduced_accel;

            if (GameMath::Fabs(reduced_force) < GameMath::Fabs(force)) {
                force = reduced_force;
            }

            const Coord3D *dir = obj->Get_Unit_Dir_Vector2D();
            Coord3D force3d;
            force3d.x = force * dir->x;
            force3d.y = force * dir->y;
            force3d.z = 0.0f;
            physics->Apply_Motive_Force(&force3d);
        }
    }
}

void Locomotor::Move_Towards_Position_Treads(
    Object *obj, PhysicsBehavior *physics, const Coord3D &goal_pos, float on_path_dist_to_goal, float desired_speed)
{
    BodyDamageType damage = obj->Get_Body_Module()->Get_Damage_State();
    float max_speed = Get_Max_Speed_For_Condition(damage);
    float max_accel = Get_Max_Acceleration(damage);

    if (desired_speed > max_speed) {
        desired_speed = max_speed;
    }

    float goal_angle;
    physics->Set_Turning(Rotate_Towards_Position(obj, goal_pos, &goal_angle));
    float acute_factor = GameMath::Fabs(goal_angle) / DEG_TO_RADF(45.0f);

    if (acute_factor > 1.0f) {
        acute_factor = 1.0f;
    }

    float x = obj->Get_Position()->x - goal_pos.x;
    float y = obj->Get_Position()->y - goal_pos.y;
    float reduced_speed = (1.0f - acute_factor) * desired_speed;
    float cur_speed = physics->Get_Forward_Speed_2D();
    float speed3 = cur_speed / Get_Braking();
    float brake_dist = cur_speed / 1.5f * speed3;

    if (GameMath::Square(20.0f) > GameMath::Square(y) + GameMath::Square(x) && acute_factor > 0.05f) {
        reduced_speed = cur_speed * 0.60000002f;
    }

    if (on_path_dist_to_goal < brake_dist && !Get_Flag(IS_BRAKING) && !Get_Flag(NO_SLOW_DOWN_AS_APPROACHING_DEST)) {
        Set_Flag(IS_BRAKING, true);
        m_brakingFactor = 1.1f;
    }

    if (on_path_dist_to_goal > 10.0f && brake_dist + brake_dist < on_path_dist_to_goal) {
        Set_Flag(IS_BRAKING, false);
    }

    if (Get_Flag(IS_BRAKING)) {
        m_brakingFactor = brake_dist / on_path_dist_to_goal;
        m_brakingFactor = m_brakingFactor * m_brakingFactor;

        if (m_brakingFactor > 5.0f) {
            m_brakingFactor = 5.0f;
        }

        if (brake_dist > on_path_dist_to_goal) {
            reduced_speed = cur_speed - Get_Braking();

            if (reduced_speed < 0.0f) {
                reduced_speed = 0.0f;
            }
        } else if (on_path_dist_to_goal * 0.75f < brake_dist) {
            reduced_speed = cur_speed - Get_Braking() / 2.0f;

            if (reduced_speed < 0.0f) {
                reduced_speed = 0.0f;
            }
        } else {
            reduced_speed = cur_speed;
        }
    }

    float reduced_accel = reduced_speed - cur_speed;

    if (reduced_accel != 0.0f) {
        float mass = physics->Get_Mass();
        float accel;

        if (reduced_accel > 0.0f) {
            accel = max_accel;
        } else {
            accel = Get_Braking() * -m_brakingFactor;
        }

        float force = mass * accel;
        float reduced_force = mass * reduced_accel;

        if (GameMath::Fabs(reduced_force) < GameMath::Fabs(force)) {
            force = reduced_force;
        }

        const Coord3D *dir = obj->Get_Unit_Dir_Vector2D();
        Coord3D force3d;
        force3d.x = force * dir->x;
        force3d.y = force * dir->y;
        force3d.z = 0.0;
        physics->Apply_Motive_Force(&force3d);
    }
}

void Locomotor::Move_Towards_Position_Other(
    Object *obj, PhysicsBehavior *physics, const Coord3D &goal_pos, float on_path_dist_to_goal, float desired_speed)
{
#ifdef GAME_DLL
    Call_Method<void, Locomotor, Object *, PhysicsBehavior *, const Coord3D &, float, float>(
        PICK_ADDRESS(0x004BC500, 0x00752932), this, obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
#endif
}

void Locomotor::Move_Towards_Position_Hover(
    Object *obj, PhysicsBehavior *physics, const Coord3D &goal_pos, float on_path_dist_to_goal, float desired_speed)
{
    Move_Towards_Position_Other(obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
    const Coord3D *pos = obj->Get_Position();

    if (g_theTerrainLogic->Is_Underwater(pos->x, pos->y, nullptr, nullptr)) {
        if (!Get_Flag(OVER_WATER)) {
            Set_Flag(OVER_WATER, true);
            obj->Set_Model_Condition_State(MODELCONDITION_OVER_WATER);
        }
    } else if (Get_Flag(OVER_WATER)) {
        Set_Flag(OVER_WATER, false);
        obj->Clear_Model_Condition_State(MODELCONDITION_OVER_WATER);
    }
}

void Locomotor::Move_Towards_Position_Thrust(
    Object *obj, PhysicsBehavior *physics, const Coord3D &goal_pos, float on_path_dist_to_goal, float desired_speed)
{
#ifdef GAME_DLL
    Call_Method<void, Locomotor, Object *, PhysicsBehavior *, const Coord3D &, float, float>(
        PICK_ADDRESS(0x004BAF80, 0x00751571), this, obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
#endif
}

void Locomotor::Move_Towards_Position_Wings(
    Object *obj, PhysicsBehavior *physics, const Coord3D &goal_pos, float on_path_dist_to_goal, float desired_speed)
{
    Move_Towards_Position_Other(obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
}

void Locomotor::Move_Towards_Position_Climb(
    Object *obj, PhysicsBehavior *physics, const Coord3D &goal_pos, float on_path_dist_to_goal, float desired_speed)
{
#ifdef GAME_DLL
    Call_Method<void, Locomotor, Object *, PhysicsBehavior *, const Coord3D &, float, float>(
        PICK_ADDRESS(0x004BAAE0, 0x00750FF4), this, obj, physics, goal_pos, on_path_dist_to_goal, desired_speed);
#endif
}

bool Locomotor::Fix_Invalid_Position(Object *obj, PhysicsBehavior *physics)
{
#ifdef GAME_DLL
    return Call_Method<bool, Locomotor, Object *, PhysicsBehavior *>(
        PICK_ADDRESS(0x004BA510, 0x007509AA), this, obj, physics);
#else
    return false;
#endif
}

void Locomotor::Handle_Behavior_Z(Object *obj, PhysicsBehavior *physics, const Coord3D &goal_pos)
{
#ifdef GAME_DLL
    Call_Method<void, Locomotor, Object *, PhysicsBehavior *, const Coord3D &>(
        PICK_ADDRESS(0x004BC0E0, 0x00752429), this, obj, physics, goal_pos);
#endif
}

PhysicsTurningType Locomotor::Rotate_Obj_Around_Loco_Pivot(Object *obj, const Coord3D &position, float rate, float *angle)
{
#ifdef GAME_DLL
    return Call_Method<PhysicsTurningType, Locomotor, Object *, const Coord3D &, float, float *>(
        PICK_ADDRESS(0x004BBCF0, 0x00752114), this, obj, position, rate, angle);
#else
    return TURN_NONE;
#endif
}
