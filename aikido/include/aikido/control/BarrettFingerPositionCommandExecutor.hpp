#ifndef AIKIDO_CONTROL_BARRETFINGERPOSITIONCOMMANDEXECUTOR_HPP_
#define AIKIDO_CONTROL_BARRETFINGERPOSITIONCOMMANDEXECUTOR_HPP_
#include <dart/collision/CollisionDetector.h>
#include <dart/collision/Option.h>
#include <dart/collision/CollisionGroup.h>
#include <dart/collision/CollisionFilter.h>
#include <dart/dynamics/dynamics.h>
#include <future>
#include <mutex>
#include <condition_variable>

namespace aikido {
namespace control {

/// This executor mimics the behavior of BarretFinger. 
/// It moves a finger to a desired point; it may stop early if 
/// joint limit is reached or collision is detected.
/// Only the primal joint is actuated; the distal joint moves with mimic ratio.
/// When collision is detected on the distal link, the finger stops.
/// When collision is detected on the primal link, the distal link moves
/// until completion or until distal collision is detected.
class BarrettFingerPositionCommandExecutor
{
public:
  /// Constructor.
  /// \param _finger Finger to be controlled by this Executor.
  /// \param _primal Index of primal dof
  /// \param _distal Index of distal dof
  /// \param _collisionDetector CollisionDetector for detecting self collision
  ///        and collision with objects. 
  /// \param _collisionOptions Default is (enableContact=false, binaryCheck=true,
  ///        maxNumContacts = 1.) 
  ///        See dart/collison/Option.h for more information 
  BarrettFingerPositionCommandExecutor(
    ::dart::dynamics::ChainPtr _finger, int _primal, int _distal,
    ::dart::collision::CollisionDetectorPtr _collisionDetector,
    ::dart::collision::Option _collisionOptions = ::dart::collision::Option(
      false, true, 1));

  /// Sets variables to move the finger joints.
  /// Primal dof moves to _goalPosition, joint limit, or until collision.
  /// Step method should be called multiple times until future returns.
  /// Distal dof follows with mimic ratio. 
  /// \param _goalPosition Desired angle of primal joint.
  /// \param _collideWith CollisionGroup to check collision with fingers.
  /// \return future Becomes available when the execution completes.
  std::future<void> execute(double _goalPosition,
    ::dart::collision::CollisionGroupPtr _collideWith);

  /// Returns mimic ratio, i.e. how much the distal joint moves relative to 
  /// the primal joint. 
  /// \return mimic ratio.
  static double getMimicRatio();

  /// Moves the joints of the finger by dofVelocity*_timeSIncePreviousCall
  /// until execute's goalPosition by primary dof or collision is detected.
  /// If primal link is in collision, distal link moves until 
  /// mimicRatio*goalPosition. If distal link is in collision, execution stops.
  /// If multiple threads are accessing this function or skeleton associated
  /// with this executor, it is necessary to lock the skeleton before
  /// calling this method.
  /// \param _timeSincePreviousCall Time since previous call. 
  void step(double _timeSincePreviousCall);

private: 
  constexpr static double kMimicRatio = 0.333; 
  constexpr static double kPrimalVelocity = 0.01;
  constexpr static double kDistalVelocity = kPrimalVelocity*kMimicRatio;

  /// If (current dof - goalPosition) execution terminates. 
  constexpr static double kTolerance = 1e-3;

  ::dart::dynamics::ChainPtr mFinger;

  /// Primal, distal dofs
  ::dart::dynamics::DegreeOfFreedom* mPrimalDof;
  ::dart::dynamics::DegreeOfFreedom* mDistalDof;

  /// Joint limits for primal and distal dof.
  std::pair<double, double> mPrimalLimits, mDistalLimits;

  ::dart::collision::CollisionDetectorPtr mCollisionDetector;
  ::dart::collision::Option mCollisionOptions;

  ::dart::collision::CollisionGroupPtr mPrimalCollisionGroup;
  ::dart::collision::CollisionGroupPtr mDistalCollisionGroup;

  std::unique_ptr<std::promise<void>> mPromise;

  /// Control access to mPromise, mInExecution, mGoalPosition, mDistalOnly,
  /// mCollideWith 
  std::mutex mMutex;

  /// Flag for indicating execution of a command. 
  bool mInExecution;

  /// Desired end-position of primal and distal dofs.
  double mPrimalGoalPosition, mDistalGoalPosition;

  /// Indicator that only distal finger is to be moved.
  bool mDistalOnly;

  ::dart::collision::CollisionGroupPtr mCollideWith;
   
};

using BarrettFingerPositionCommandExecutorPtr = std::shared_ptr<BarrettFingerPositionCommandExecutor>;


} // control
} // aikido

#endif
