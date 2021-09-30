#pragma once

#include <Kin/kin.h>
#include <KOMO/komo.h>

//===========================================================================


//===========================================================================

arr getLoopPath(rai::Configuration& C);
void addBoxPickObjectives_botop(KOMO& komo, double time, rai::ArgWord dir, const char* boxName, const arr& boxSize, const char* gripperName, const char* palmName, const char* tableName);
void addBoxPlaceObjectives_botop(KOMO& komo, double time, rai::ArgWord dir, const char* boxName, const arr& boxSize, const char* gripperName, const char* palmName);
arr getBoxPnpKeyframes(const rai::Configuration& C,
                       rai::ArgWord pickDirection, rai::ArgWord placeDirection,
                       const char* boxName, const char* gripperName, const char* palmName, const char* tableName,
                       const arr& qHome);

//===========================================================================

arr getLoopPath(rai::Configuration& C);
