#include <BotOp/bot.h>
#include <BotOp/SequenceController.h>
#include <KOMO/manipTools.h>
#include <KOMO/pathTools.h>
#include <Kin/viewer.h>

#include <Kin/F_forces.h>

//===========================================================================

struct SequenceControllerExperiment{
  rai::Configuration& C;
  arr qHome;
  unique_ptr<SequenceController> ctrl;
  unique_ptr<BotOp> bot;
  Metronome tic;
  uint stepCount = 0;

  SequenceControllerExperiment(rai::Configuration& _C, ObjectiveL& phi, double cycleTime=.1) : C(_C), tic(cycleTime){
    qHome = C.getJointState();
    ctrl = make_unique<SequenceController>(C, phi, qHome);
  }

  SequenceControllerExperiment(rai::Configuration& _C, const KOMO& komo, double cycleTime=.1) : C(_C), tic(cycleTime){
    qHome = C.getJointState();
    ctrl = make_unique<SequenceController>(C, komo, qHome);
  }

  bool step(ObjectiveL& phi){
    stepCount++;

    //-- start a robot thread
    if(!bot){
      bot = make_unique<BotOp>(C, rai::checkParameter<bool>("real"));
      bot->home(C);
      bot->setControllerWriteData(1);
      rai::wait(.2);
    }

    //-- iterate
    tic.waitForTic();

    //-- get optitrack
    if(bot->optitrack) bot->optitrack->pull(C);

    //-- get current state (time,q,qDot)
    arr q,qDot, q_ref, qDot_ref;
    double ctrlTime = 0.;
    bot->getState(q, qDot, ctrlTime);
    bot->getReference(q_ref, qDot_ref, NoArr, q, qDot, ctrlTime);

    //-- iterate MPC
    ctrl->cycle(C, phi, q_ref, qDot_ref, q, qDot, ctrlTime);
    ctrl->report(C, phi);

    //-- send leap target
    auto sp = ctrl->getSpline(bot->get_t());
    if(sp.pts.N) bot->move(sp.pts, sp.vels, sp.times, true);

    //-- update C
    bot->step(C, .0);
    if(bot->keypressed=='q' || bot->keypressed==27) return false;

    return true;
  }

};

//===========================================================================

void testBallFollowing() {
  rai::Configuration C;
  C.addFile(rai::raiPath("../rai-robotModels/scenarios/pandasTable-calibrated.g"));

  C.addFrame("ball", "table")
      ->setShape(rai::ST_sphere, {.03})
      .setColor({1.,0,0})
      .setRelativePosition(arr{-.4,.4,.4});

  KOMO komo;
  komo.setModel(C, false);
  komo.setTiming(1., 1, 5., 1);
  komo.add_qControlObjective({}, 1, 1e-1);
  komo.addQuaternionNorms();

  komo.addObjective({1.}, FS_positionDiff, {"l_gripper", "ball"}, OT_eq, {1e1});

  komo.optimize();

  komo.getReport(true);
  komo.view(true, "optimized motion");
  while(komo.view_play(true));

  SequenceControllerExperiment ex(C, komo, .02);

  bool useSimulatedBall=!rai::getParameter<bool>("bot/useOptitrack", false);
  arr ballVel = {.0, .0, .0};
  arr ballCen = C["ball"]->getPosition();

  while(ex.step(komo.objectives)){
    if(useSimulatedBall){
      if(!(ex.stepCount%20)){
        ballVel(0) = .01 * rnd.gauss();
        ballVel(2) = .01 * rnd.gauss();
      }
      if(!(ex.stepCount%40)) ballVel=0.;
      arr pos = C["ball"]->getPosition();
      pos += ballVel;
      pos = ballCen + .95 * (pos - ballCen);
      C["ball"]->setPosition(pos);
    }else{
      C["ball"]->setPosition(C["HandStick"]->getPosition());
    }
  }
}

//===========================================================================

void testBallReaching() {
  rai::Configuration C;
  C.addFile(rai::raiPath("../rai-robotModels/scenarios/pandasTable-calibrated.g"));

  C.addFrame("ball", "table")
      ->setShape(rai::ST_sphere, {.03})
      .setColor({1.,0,0})
      .setRelativePosition(arr{-.4,.4,.4});

  //-- define constraints
  KOMO komo;
  komo.setModel(C, false);
  komo.setTiming(2., 1, 5., 1);
  komo.add_qControlObjective({}, 1, 1e-1);
  komo.addQuaternionNorms();
  komo.addObjective({1.}, FS_positionRel, {"ball", "l_gripper"}, OT_eq, {3e1}, {0., 0., -.1});
  komo.addObjective({1., 2.}, FS_positionRel, {"ball", "l_gripper"}, OT_eq, {{2,3},{1e1,0,0,0,1e1,0}});
  komo.addObjective({2.}, FS_positionDiff, {"l_gripper", "ball"}, OT_eq, {3e1});

  SequenceControllerExperiment ex(C, komo, .02);

  bool useSimulatedBall=!rai::getParameter<bool>("bot/useOptitrack", false);
  arr ballVel = {.0, .0, .0};
  arr ballCen = C["ball"]->getPosition();

  while(ex.step(komo.objectives)){
    if(useSimulatedBall){
      if(!(ex.stepCount%20)){
        ballVel(0) = .01 * rnd.gauss();
        ballVel(2) = .01 * rnd.gauss();
      }
      if(!(ex.stepCount%40)) ballVel=0.;
      arr pos = C["ball"]->getPosition();
      pos += ballVel;
      pos = ballCen + .95 * (pos - ballCen);
      C["ball"]->setPosition(pos);
    }else{
      C["ball"]->setPosition(C["HandStick"]->getPosition());
    }
  }
}

//===========================================================================

void testPnp() {
  rai::Configuration C;
  C.addFile(rai::raiPath("../rai-robotModels/scenarios/pandasTable-calibrated.g"));

  C.addFrame("box", "table")
      ->setJoint(rai::JT_rigid)
      .setShape(rai::ST_ssBox, {.06,.15,.09,.01})
      .setRelativePosition(arr{-.0,-.0,.095});

  C.addFrame("target", "table")
      ->setJoint(rai::JT_rigid)
      .setShape(rai::ST_ssBox, {.4,.4,.1,.01})
      .setRelativePosition(arr{-.4,.2,.0});

  //-- define constraints
  ObjectiveL phi;
  addBoxPickObjectives(phi, C, 1., rai::_xAxis, "box", "l_gripper", "l_palm", "target");

  SequenceControllerExperiment ex(C, phi);

  while(ex.step(phi));
}

//===========================================================================

void testPushing() {
  rai::Configuration C;
  C.addFile("pushScenario.g");


  //-- define constraints
  ObjectiveL phi;
  //phi.add({1.}, FS_positionDiff, C, {"stickTip", "puck"}, OT_eq, {1e1});
  phi.add({1.}, make_shared<F_PushRadiusPrior>(.13), C, {"stickTip", "puck", "target"}, OT_eq, {1e1}, {0., 0., .08});
//  phi.add({1.}, FS_positionDiff, C, {"stickTip", "puck"}, OT_eq, {{1,3},{0.,0.,1e1}, {0., 0., .1});
  phi.add({2.}, make_shared<F_PushRadiusPrior>(.02), C, {"stickTip", "puck", "target"}, OT_eq, {1e1});
  phi.add({1., 2.}, make_shared<F_PushAligned>(), C, {"stickTip", "puck", "target"}, OT_eq, {{1,3},{0,0,1e1}});
  //phi.add({1., 2.}, FS_positionRel, C, {"ball", "l_gripper"}, OT_eq, {{2,3},{1e1,0,0,0,1e1,0}});
  //phi.add({2.}, FS_positionDiff, C, {"l_gripper", "ball"}, OT_eq, {1e1});

//  komo.addContact_slide(s.phase0, s.phase1, s.frames(0), s.frames(1));
//  if(s.phase1>=s.phase0+.8){
//    rai::Frame* obj = komo.world.getFrame(s.frames(1));
//    if(!(obj->shape && obj->shape->type()==ST_sphere) && obj->children.N){
//      obj = obj->children.last();
//    }
//    if(obj->shape && obj->shape->type()==ST_sphere){
//      double rad = obj->shape->radius();
//      arr times = {s.phase0+.2,s.phase1-.2};
//      if(komo.k_order==1) times = {s.phase0, s.phase1};
//      komo.addObjective(times, make_shared<F_PushRadiusPrior>(rad), s.frames, OT_sos, {1e1}, NoArr, 1, +1, 0);
//    }
//  }
//  if(komo.k_order>1){
//    komo.addObjective({s.phase0, s.phase1}, FS_position, {s.frames(1)}, OT_sos, {3e0}, {}, 2); //smooth obj motion
//    komo.addObjective({s.phase1}, FS_pose, {s.frames(0)}, OT_eq, {1e0}, {}, 1);
//    komo.addObjective({s.phase1}, FS_pose, {s.frames(1)}, OT_eq, {1e0}, {}, 1);
//  }

  bool useOptitrack=rai::getParameter<bool>("bot/useOptitrack", false);

  SequenceControllerExperiment ex(C);
  while(ex.step(phi)){
    if(useOptitrack){
      C["puck"]->setPosition(C["b1"]->getPosition());
    }
  }
}

//===========================================================================

void testDroneRace(){
  rai::Configuration C;
  C.addFile("droneRace.g");

  //-- define constraints
  KOMO komo;
  komo.setModel(C, false);
  komo.setTiming(7., 1, 5., 1);
  komo.add_qControlObjective({}, 1, 1e-1);
  komo.addQuaternionNorms();
  komo.addObjective({1.}, FS_positionDiff, {"drone", "target0"}, OT_eq, {1e1});
  komo.addObjective({2.}, FS_positionDiff, {"drone", "target1"}, OT_eq, {1e1});
  komo.addObjective({3.}, FS_positionDiff, {"drone", "target2"}, OT_eq, {1e1});
  komo.addObjective({4.}, FS_positionDiff, {"drone", "target3"}, OT_eq, {1e1});
  komo.addObjective({5.}, FS_positionDiff, {"drone", "target0"}, OT_eq, {1e1});
  komo.addObjective({6.}, FS_positionDiff, {"drone", "target1"}, OT_eq, {1e1});
  komo.addObjective({7.}, FS_position, {"drone"}, OT_eq, {1e1}, {0,-.5, 1.});


  //-- not yet integrated
  //SequenceControllerExperiment ex(C, komo);
  //while(ex.step(komo.objectives));

  //-- manually just optimize once and dump spline

  //optimize keyframes
  komo.optimize();
  komo.getReport(true);
  komo.view(false, "optimized motion");
  arr keyframes = komo.getPath_qOrg();

  //optimize timing
  TimingMPC F(keyframes, 1e0, 10); //last number (ctrlCost) in range [1,10] from fast-slow
  arr x0 = C["drone"]->getPosition();
  arr v0 = zeros(3);
  F.solve(x0, v0);

  //get spline
  rai::CubicSpline S;
  F.getCubicSpline(S, x0, v0);

  //display
  rai::Mesh M;
  M.V = S.eval(range(0., S.times.last(), 100));
  M.makeLineStrip();
  C.gl()->add(M);
  C.watch(true);

  //analyze only to plot the max vel/acc
  arr path = S.eval(range(0., S.times.last(), 100));
  double tau = S.times.last()/100.;
  arr ttau = consts<double>(tau, 101);
  double maxVel=1., maxAcc=1., maxJer=30.;
  arr time(path.d0);
  time.setZero();
  for(uint t=1;t<time.N;t++) time(t) = time(t-1) + ttau(t);

  arr v = max(getVel(path,ttau),1) / maxVel;
  arr a = max(getAcc(path,ttau),1) / maxAcc;
  arr j = max(getJerk(path,ttau),1) / maxJer;
  arr vi = min(getVel(path,ttau),1) / maxVel;
  arr ai = min(getAcc(path,ttau),1) / maxAcc;
  arr ji = min(getJerk(path,ttau),1) / maxJer;
  catCol(LIST(~~time, ~~v, ~~a, ~~j, ~~vi, ~~ai, ~~ji)).reshape(-1,7).writeRaw( FILE("z.dat") );
  gnuplot("plot [:][-1.1:1.1] 'z.dat' us 1:2 t 'v', '' us 1:3 t 'a', '' us 1:5 t 'vmin', '' us 1:6 t 'amin'"); //, '' us 1:4 t 'j', , '' us 1:7 t 'jmin'


  //just sample & dump the spline
  for(double t=0;t<S.times.last();t += .01){
    //time 3-positions 3-velocities
    cout <<t <<S.eval(t).modRaw() <<' ' <<S.eval(t,1).modRaw() <<endl;
  }

}

//===========================================================================


int main(int argc, char * argv[]){
  rai::initCmdLine(argc, argv);

  //  rnd.clockSeed();
  rnd.seed(1);

  //testBallFollowing();
  //testBallReaching();
  //testPnp();
  //testPushing();
  testDroneRace();


  LOG(0) <<" === bye bye ===\n used parameters:\n" <<rai::getParameters()() <<'\n';

  return 0;
}
