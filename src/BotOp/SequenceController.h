#pragma once

#include <KOMO/komo.h>
#include <Control/timingMPC.h>

//===========================================================================

//A wrapper of KOMO to optimize waypoints for a given sequence of constraints
struct WaypointMPC{
  KOMO komo;
  arr qHome;
  uint steps=0;
  //result
  arr path;
  arr tau;
  bool feasible=false;

  WaypointMPC(rai::Configuration& C, const rai::Array<ObjectiveL>& _phiflag, const rai::Array<ObjectiveL>& _phirun, const arr& qHome={});
  WaypointMPC(rai::Configuration& C, const ObjectiveL& phi, const arr& qHome={});
  WaypointMPC(rai::Configuration& C, const KOMO& _komo, const arr& qHome={});

  void reinit(const rai::Configuration& C);
  void solve();
};

//===========================================================================

struct SequenceController{
  WaypointMPC pathMPC;
  TimingMPC timingMPC;
  rai::String msg;
  double precision = .1;
  double ctrlTimeLast = -1.;
  double tauCutoff = -.1;

  SequenceController(rai::Configuration& C, const rai::Array<ObjectiveL>& _phiflag, const rai::Array<ObjectiveL>& _phirun, const arr& qHome={});
  SequenceController(rai::Configuration& C, const ObjectiveL& phi, const arr& qHome={}, double timeCost=1e0, double ctrlCost=1e0);
  SequenceController(rai::Configuration& C, const KOMO& komo, const arr& qHome={}, double timeCost=1e0, double ctrlCost=1e0);

  void updateWaypoints(const rai::Configuration& C);
  void updateTiming(const rai::Configuration& C, const ObjectiveL& phi, double ctrlTime, const arr& q_real, const arr& qDot_real, const arr& q_ref, const arr& qDot_ref);
  void cycle(const rai::Configuration& C, const ObjectiveL& phi, const arr& q_ref, const arr& qDot_ref, const arr& q_real, const arr& qDot_real, double ctrlTime);
  rai::CubicSplineCtor getSpline(double realtime);
  void report(const rai::Configuration& C, const rai::Array<ObjectiveL>& phiflag, const rai::Array<ObjectiveL>& phirun);
  void report(const rai::Configuration& C, const ObjectiveL& phi);
};






