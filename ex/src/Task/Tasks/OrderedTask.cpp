/* Generated by Together */

#include "OrderedTask.hpp"
#include "BaseTask/OrderedTaskPoint.hpp"
#include "PathSolvers/TaskDijkstra.hpp"
#include "TaskSolvers/TaskMacCreadyTravelled.hpp"
#include "TaskSolvers/TaskMacCreadyRemaining.hpp"
#include "TaskSolvers/TaskMacCreadyTotal.hpp"
#include "TaskSolvers/TaskCruiseEfficiency.hpp"
#include "TaskSolvers/TaskBestMc.hpp"
#include "TaskSolvers/TaskMinTarget.hpp"
#include "TaskSolvers/TaskGlideRequired.hpp"
#include "TaskSolvers/TaskOptTarget.hpp"
#include <assert.h>
#include "Task/Visitors/TaskPointVisitor.hpp"

void
OrderedTask::update_geometry() {
  for (unsigned i=0; i<tps.size(); i++) {
    tps[i]->update_geometry();
    tps[i]->clear_boundary_points();
    tps[i]->default_boundary_points();
    tps[i]->prune_boundary_points();
    tps[i]->update_projection();
  }
}

////////// TIMES

double 
OrderedTask::scan_total_start_time(const AIRCRAFT_STATE &)
{
  if (ts) {
    return ts->get_state_entered().Time;
  } else {
    return 0.0;
  }
}

double 
OrderedTask::scan_leg_start_time(const AIRCRAFT_STATE &)
{
  if (activeTaskPoint>0) {
    return tps[activeTaskPoint-1]->get_state_entered().Time;
  } else {
    return -1;
  }
}

////////// DISTANCES

void
OrderedTask::scan_distance_minmax(const GEOPOINT &location, bool full,
                                  double *dmin, double *dmax)
{
  if (!ts) {
    return;
  }
  SearchPoint ac(location, task_projection);
  if (full) {
    // for max calculations, since one can still travel further in the sector,
    // we pretend we are on the previous turnpoint so the search samples will
    // contain the full boundary
    unsigned atp = activeTaskPoint;
    if (activeTaskPoint>0) {
      activeTaskPoint--;
      ts->scan_active(tps[activeTaskPoint]);
    }
    TaskDijkstra dijkstra_max(this, tps.size());
    dijkstra_max.distance_max();

    activeTaskPoint = atp;
    ts->scan_active(tps[activeTaskPoint]);
    *dmax = ts->scan_distance_max();
  }
  TaskDijkstra dijkstra_min(this, tps.size());
  dijkstra_min.distance_min(ac);
  *dmin = ts->scan_distance_min();
}


double
OrderedTask::scan_distance_nominal()
{
  if (ts) {
    return ts->scan_distance_nominal();
  } else {
    return 0;
  }
}

double
OrderedTask::scan_distance_scored(const GEOPOINT &location)
{
  if (ts) {
    return ts->scan_distance_scored(location);
  } else {
    return 0;
  }
}

double
OrderedTask::scan_distance_remaining(const GEOPOINT &location)
{
  if (ts) {
    return ts->scan_distance_remaining(location);
  } else {
    return 0;
  }
}

double
OrderedTask::scan_distance_travelled(const GEOPOINT &location)
{
  if (ts) {
    return ts->scan_distance_travelled(location);
  } else {
    return 0;
  }
}

double
OrderedTask::scan_distance_planned()
{
  if (ts) {
    return ts->scan_distance_planned();
  } else {
    return 0;
  }
}

////////// TRANSITIONS

bool 
OrderedTask::check_transitions(const AIRCRAFT_STATE &state, 
                               const AIRCRAFT_STATE &state_last)
{
  if (!ts) {
    return false;
  }
  ts->scan_active(tps[activeTaskPoint]);

  const int n_task = tps.size();

  if (!n_task) {
    return false;
  }

  const int t_min = std::max(0,(int)activeTaskPoint-1);
  const int t_max = std::min(n_task-1, (int)activeTaskPoint+1);
  bool full_update = false;
  
  for (int i=t_min; i<=t_max; i++) {
    bool transition_enter = false;
    if (tps[i]->transition_enter(state, state_last)) {
      transition_enter = true;
      task_events.transition_enter(*tps[i]);
    }
    bool transition_exit = false;
    if (tps[i]->transition_exit(state, state_last)) {
      transition_exit = true;
      task_events.transition_exit(*tps[i]);
    }

    if ((i==(int)activeTaskPoint) && 
      task_advance.ready_to_advance(*tps[i],
                                    state,
                                    transition_enter,
                                    transition_exit)) {
      task_advance.set_armed(false);

      if (i+1<n_task) {

        i++;
        setActiveTaskPoint(i);
        ts->scan_active(tps[activeTaskPoint]);

        if (tps[i]->update_sample(state)) {
          full_update = true;
        }

        task_events.active_advanced(*tps[i],i);

        // on sector exit, must update samples since start sector
        // exit transition clears samples
        full_update = true;
      }
    }

    if (tps[i]->update_sample(state)) {
      full_update = true;
    }
  }

  ts->scan_active(tps[activeTaskPoint]);

  return full_update;
}

////////// ADDITIONAL FUNCTIONS

bool 
OrderedTask::update_idle(const AIRCRAFT_STATE& state)
{
  bool retval = AbstractTask::update_idle(state);

  if (ts && task_behaviour.optimise_targets_range 
      && (task_behaviour.aat_min_time>0.0)) {
    if (activeTaskPoint>0) {
      double p = calc_min_target(state, task_behaviour.aat_min_time);
      (void)p;

      if (task_behaviour.optimise_targets_bearing) {
        if (AATPoint* ap = dynamic_cast<AATPoint*>(tps[activeTaskPoint])) {
          // very nasty hack
          TaskOptTarget tot(tps, activeTaskPoint, state, glide_polar,
                            *ap, ts);
          
          tot.search(0.5);
        }
      }
    }
    retval = true;
  }
  
  return retval;
}


bool 
OrderedTask::update_sample(const AIRCRAFT_STATE &state, 
                           const bool full_update)
{
  if (activeTaskPoint==0) {
    stats.reset();
  }
  return true;
}

////////// TASK 

void
OrderedTask::set_neighbours(unsigned position)
{
  OrderedTaskPoint* prev=NULL;
  OrderedTaskPoint* next=NULL;

  if (position>=tps.size()) {
    // nothing to do
    return;
  }

  if (position>0) {
    prev = tps[position-1];
  }
  if (position+1<tps.size()) {
    next = tps[position+1];
  }
  tps[position]->set_neighbours(prev, next);
}


bool
OrderedTask::check_task() const
{
  if (!tps.size()) {
    task_events.construction_error("Error! Empty task\n");
    return false;
  }
  if (!dynamic_cast<const StartPoint*>(tps[0])) {
    task_events.construction_error("Error! No start point\n");
    return false;
  }
  if (!dynamic_cast<const FinishPoint*>(tps[tps.size()-1])) {
    task_events.construction_error("Error! No finish point\n");
    return false;
  }
  return true;
}


bool 
OrderedTask::check_startfinish(OrderedTaskPoint* new_tp)
{
  if (StartPoint* ap = dynamic_cast<StartPoint*>(new_tp)) {
    if (tps.size()) {
      task_events.construction_error("Error! Already has a start point\n");
      return false;
    } else {
      ts = ap;
    }
  }
  if (FinishPoint* fp = dynamic_cast<FinishPoint*>(new_tp)) {
    if (tf) {
      task_events.construction_error("Error! Already has a finish point\n");
      return false;
    } else {
      tf = fp;
    }
  }
  return true;
}


bool
OrderedTask::remove(unsigned position)
{
  // for now, don't allow removing start/finish
  assert(position>0);
  assert(position+1<tps.size());

  if (activeTaskPoint>position) {
    activeTaskPoint--;
  }

  delete tps[position];
  tps.erase(tps.begin()+position); // 0,1,2,3 -> 0,1,3

  set_neighbours(position);
  if (position) {
    set_neighbours(position-1);
  }
  update_geometry();
  return true;
}


bool 
OrderedTask::append(OrderedTaskPoint* new_tp)
{
  if (!check_startfinish(new_tp)) {
    return false;
  }

  tps.push_back(new_tp);
  if (tps.size()>1) {
    set_neighbours(tps.size()-2);
  }
  set_neighbours(tps.size()-1);
  update_geometry();
  return true;
}

bool 
OrderedTask::insert(OrderedTaskPoint* new_tp, unsigned position)
{
  if (position) {
    task_events.construction_error("Error! can't insert at start\n");
    return false;
  }
  if (!check_startfinish(new_tp)) {
    return false;
  }

  // inserts at position
  if (activeTaskPoint>=position) {
    activeTaskPoint++;
  }
  // example, position=1

  if (position<tps.size()) {
    tps.insert(tps.begin()+position, new_tp); // 0,1,2 -> 0,N,1,2
  } else {
    tps.push_back(new_tp);
  }
  // need to update 1,2,3
  set_neighbours(position-1);
  set_neighbours(position);
  set_neighbours(position+1);
  
  update_geometry();
  return true;
}

//////////  

void OrderedTask::setActiveTaskPoint(unsigned index)
{
  if (index<tps.size()) {
    activeTaskPoint = index;
  }
}

TaskPoint* OrderedTask::getActiveTaskPoint()
{
  if (activeTaskPoint<tps.size()) {
    return tps[activeTaskPoint];
  } else {
    return NULL;
  }
}

////////// Glide functions

void
OrderedTask::glide_solution_remaining(const AIRCRAFT_STATE &aircraft, 
                                      GlideResult &total,
                                      GlideResult &leg)
{
  TaskMacCreadyRemaining tm(tps,activeTaskPoint,glide_polar);
  total = tm.glide_solution(aircraft);
  leg = tm.get_active_solution();
}

void
OrderedTask::glide_solution_travelled(const AIRCRAFT_STATE &aircraft, 
                                      GlideResult &total,
                                      GlideResult &leg)
{
  TaskMacCreadyTravelled tm(tps,activeTaskPoint,glide_polar);
  total = tm.glide_solution(aircraft);
  leg = tm.get_active_solution();
}

void
OrderedTask::glide_solution_planned(const AIRCRAFT_STATE &aircraft, 
                                    GlideResult &total,
                                    GlideResult &leg,
                                    DistanceRemainingStat &total_remaining_effective,
                                    DistanceRemainingStat &leg_remaining_effective,
                                    const double total_t_elapsed,
                                    const double leg_t_elapsed)
{
  TaskMacCreadyTotal tm(tps,activeTaskPoint,glide_polar);
  total = tm.glide_solution(aircraft);
  leg = tm.get_active_solution();

  total_remaining_effective.
    set_distance(tm.effective_distance(total_t_elapsed));

  leg_remaining_effective.
    set_distance(tm.effective_leg_distance(leg_t_elapsed));
}

////////// Auxiliary glide functions

double
OrderedTask::calc_glide_required(const AIRCRAFT_STATE &aircraft) 
{
  TaskGlideRequired bgr(tps, activeTaskPoint, aircraft, glide_polar);
  return bgr.search(0.0);
}

double
OrderedTask::calc_mc_best(const AIRCRAFT_STATE &aircraft)
{
  // note setting of lower limit on mc
  TaskBestMc bmc(tps,activeTaskPoint, aircraft, glide_polar);
  return bmc.search(glide_polar.get_mc());
}


double
OrderedTask::calc_cruise_efficiency(const AIRCRAFT_STATE &aircraft)
{
  if (activeTaskPoint>0) {
    TaskCruiseEfficiency bce(tps,activeTaskPoint, aircraft, glide_polar);
    return bce.search(1.0);
  } else {
    return 1.0;
  }
}

double
OrderedTask::calc_min_target(const AIRCRAFT_STATE &aircraft, 
                             const double t_target)
{
  // TODO: look at max/min dist and only perform this scan if
  // change is possible
  const double t_rem = std::max(0.0, t_target-stats.total.TimeElapsed);

  TaskMinTarget bmt(tps, activeTaskPoint, aircraft, glide_polar, t_rem, ts);
  double p= bmt.search(0.0);
  return p;
}


double 
OrderedTask::calc_gradient(const AIRCRAFT_STATE &state) 
{
  double g_best = 0.0;
  double d_acc = 0.0;
  double h_this = state.Altitude;

  for (unsigned i=activeTaskPoint; i< tps.size(); i++) {
    d_acc += tps[i]->get_vector_remaining(state).Distance;
    if (!d_acc) {
      continue;
    }
    const double g_this = (h_this-tps[i]->getElevation())/d_acc;
    if (i==activeTaskPoint) {
      g_best = g_this;
    } else {
      g_best = std::min(g_best, g_this);
    }
  }
  return g_best;
}


////////// Constructors/destructors

OrderedTask::~OrderedTask()
{
  for (std::vector<OrderedTaskPoint*>::iterator v=tps.begin();
       v != tps.end(); ) {
    delete *v;
    tps.erase(v);
  }
}


OrderedTask::OrderedTask(const TaskEvents &te, 
                         const TaskBehaviour &tb,
                         const TaskProjection &tp,
                         TaskAdvance &ta,
                         GlidePolar &gp):
  AbstractTask(te, tb, ta, gp),
  task_projection(tp),
  ts(NULL),
  tf(NULL)
{
  // TODO: default values in constructor
}

////////////////////////

void 
OrderedTask::Accept(TaskPointVisitor& visitor) const
{
  for (std::vector<OrderedTaskPoint*>::const_iterator 
         i= tps.begin(); i!= tps.end(); i++) {
    (*i)->Accept(visitor);
  }
}
