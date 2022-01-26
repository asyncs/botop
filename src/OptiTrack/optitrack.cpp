#include "optitrack.h"

#include "motioncapture_optitrack.h"

#include <Kin/frame.h>


rai::Transformation rb2pose(const libmotioncapture::RigidBody& rb){
  rai::Transformation X;
  X.pos.set(rb.position()(0), rb.position()(1), rb.position()(2));
  X.rot.set(rb.rotation().w(), rb.rotation().vec()(0), rb.rotation().vec()(1), rb.rotation().vec()(2));
  return X;
}

void poseFilter(rai::Transformation& X, double alpha, const rai::Transformation& signal){
  X.pos = (1.-alpha)*X.pos + alpha*signal.pos;
  X.rot.setInterpolate(alpha, X.rot, signal.rot);
}

namespace rai{

  OptiTrack::OptiTrack() : Thread("OptitrackThread"){
    mocap = libmotioncapture::MotionCapture::connect("optitrack", rai::getParameter<rai::String>("optitrack/host", "130.149.82.29").p);
    threadLoop();
  }

  OptiTrack::~OptiTrack(){
    threadClose();
    delete mocap;
  }

  void OptiTrack::pull(rai::Configuration& C){
    //-- update configuration
    //we need a base frame
    const char* name = "optitrack_base";
    rai::Frame *base = C.getFrame(name, false);
    if(!base){
      LOG(0) <<"creating new frame 'optitrack_base'";
      base = C.addFrame(name);
      base->setShape(rai::ST_marker, {.3});
      base->setColor({.8, .8, .2});
      if(rai::checkParameter<arr>("optitrack_baseFrame")){
        arr X = rai::getParameter<arr>("optitrack_baseFrame");
        base->setPose(rai::Transformation(X));
      }else{
        LOG(0) <<"WARNING: optitrack_baseFrame not set - using non-calibrated default [0 0 0]!";
      }
    }

    //loop through all ot signals
    for (auto const& item: poses) {
      //get (or create) frame for that body
      const char* name = item.first;
      rai::Frame *f = C.getFrame(name, false);
      if(!f){//create a new marker frame!
        LOG(0) <<"creating new frame '" <<name <<"'";
        f = C.addFrame(name);
        f->setParent(base);
        f->setShape(rai::ST_marker, {.1});
        f->setColor({.8, .8, .2, .5});
      }

      //set pose of frame
      f->set_Q() = item.second.pose;
    }
  }

  void OptiTrack::step(){
    // Get a frame
    mocap->waitForNextFrame();
    //uint64_t timeStamp = mocap->timeStamp();

    std::map<std::string, libmotioncapture::RigidBody> rigidBodies = mocap->rigidBodies();

    std::lock_guard<std::mutex> lock(mux);
    //loop through all ot signals
    for (auto const& item: rigidBodies) {
      if(item.second.occluded()) continue;

      //get (or create) frame for that body
      const char* name = item.first.c_str();

      auto entry = poses.find(name);
      if(entry!=poses.end()){  //filter the entry
        double alpha=0.1;
        poseFilter(entry->second.pose, alpha, rb2pose(item.second));
      }else{
        //create a new entry
        poses[name] = { 0, rb2pose(item.second) };
      }
    }
  }

} //namespace
