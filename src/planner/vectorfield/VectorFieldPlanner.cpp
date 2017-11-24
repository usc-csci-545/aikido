#include <exception>
#include <string>
#include <boost/numeric/odeint.hpp>
#include <aikido/planner/vectorfield/MoveEndEffectorOffsetVectorField.hpp>
#include <aikido/planner/vectorfield/MoveEndEffectorPoseVectorField.hpp>
#include <aikido/planner/vectorfield/VectorFieldPlanner.hpp>
#include <aikido/planner/vectorfield/VectorFieldPlannerExceptions.hpp>
#include <aikido/planner/vectorfield/VectorFieldUtil.hpp>
#include <aikido/statespace/dart/MetaSkeletonStateSpaceSaver.hpp>
#include <aikido/trajectory/Spline.hpp>

using aikido::statespace::dart::MetaSkeletonStateSpaceSaver;

namespace aikido {
namespace planner {
namespace vectorfield {

static void checkDofLimits(
    const aikido::statespace::dart::MetaSkeletonStateSpacePtr& _stateSpace,
    Eigen::VectorXd const& _q,
    Eigen::VectorXd const& _qd)
{
  using dart::dynamics::DegreeOfFreedom;
  std::stringstream ss;

  for (std::size_t i = 0; i < _stateSpace->getMetaSkeleton()->getNumDofs(); ++i)
  {
    DegreeOfFreedom* const dof = _stateSpace->getMetaSkeleton()->getDof(i);

    if (_q[i] < dof->getPositionLowerLimit())
    {
      ss << "DOF " << dof->getName() << " exceeds lower position limit: ";
      ss << _q[i] << " < " << dof->getPositionLowerLimit();
      throw DofLimitError(dof, ss.str());
    }
    else if (_q[i] > dof->getPositionUpperLimit())
    {
      ss << "DOF " << dof->getName() << " exceeds upper position limit: ";
      ss << _q[i] << " > " << dof->getPositionLowerLimit();
      throw DofLimitError(dof, ss.str());
    }
    else if (_qd[i] < dof->getVelocityLowerLimit())
    {
      ss << "DOF " << dof->getName() << " exceeds lower velocity limit: ";
      ss << _qd[i] << " < " << dof->getVelocityLowerLimit();
      throw DofLimitError(dof, ss.str());
    }
    else if (_qd[i] > dof->getVelocityUpperLimit())
    {
      ss << "DOF " << dof->getName() << " exceeds upper velocity limit: ";
      ss << _qd[i] << " > " << dof->getVelocityUpperLimit();
      throw DofLimitError(dof, ss.str());
    }
  }
}

//==============================================================================
static void checkCollision(
    const aikido::statespace::dart::MetaSkeletonStateSpacePtr& _stateSpace,
    const aikido::constraint::TestablePtr& _constraint)
{
  // Get current position
  auto state = _stateSpace->getScopedStateFromMetaSkeleton();
  // Throw a termination if in collision
  if (!_constraint->isSatisfied(state))
  {
    throw VectorFieldTerminated("state in collision");
  }
}

VectorFieldPlanner::VectorFieldPlanner(
    const aikido::planner::vectorfield::ConfigurationSpaceVectorFieldPtr
        _vectorField,
    const aikido::constraint::TestablePtr _constraint,
    double _initialStepSize)
  : mVectorField(_vectorField)
  , mConstraint(_constraint)
  , mMetaSkeletonStateSpace(_vectorField->getMetaSkeletonStateSpace())
  , mMetaSkeleton(_vectorField->getMetaSkeleton())
  , mBodyNode(_vectorField->getBodyNode())
  , mInitialStepSize(_initialStepSize)
{
  mCacheIndex = -1;
  mIndex = 0;
}

void VectorFieldPlanner::step(
    const Eigen::VectorXd& q, Eigen::VectorXd& qd, double t)
{
  // set joint values
  mMetaSkeleton->setPositions(q);

  // collision checking of current joint values
  checkCollision(mMetaSkeletonStateSpace, mConstraint);

  // compute joint velocities
  bool success = mVectorField->getJointVelocities(q, t, qd);
  if (success == false)
  {
    throw IntegrationFailedException();
  }

  checkDofLimits(mMetaSkeletonStateSpace, q, qd);
}

void VectorFieldPlanner::check(const Eigen::VectorXd& q, double t)
{
  if (mTimer.getElapsedTime() > mTimelimit)
  {
    // throw TimeLimitError();
  }

  // set joint values
  mMetaSkeleton->setPositions(q);

  // collision checking of current joint values
  checkCollision(mMetaSkeletonStateSpace, mConstraint);

  Knot knot;
  knot.mT = t;
  knot.mPositions = q;

  if (mKnots.size() > 0 && t == mKnots.back().mT)
  {
    std::cout << "SAME T" << std::endl;
  }
  else
  {
    mKnots.push_back(knot);
    mIndex += 1;
  }

  VectorFieldPlannerStatus status = mVectorField->checkPlanningStatus(q, t);
  if (status == VectorFieldPlannerStatus::CACHE_AND_CONTINUE
      || status == VectorFieldPlannerStatus::CACHE_AND_TERMINATE)
  {
    mCacheIndex = mIndex;
  }

  if (status == VectorFieldPlannerStatus::TERMINATE
      || status == VectorFieldPlannerStatus::CACHE_AND_TERMINATE)
  {
    throw VectorFieldTerminated(
        "Planning was terminated by the StatusCallback.");
  }
}

std::unique_ptr<aikido::trajectory::Spline>
VectorFieldPlanner::followVectorField(
    double _integrationTimeInterval, double _timelimit)
{
  using namespace std::placeholders;
  using errorStepper = boost::numeric::odeint::
      runge_kutta_dopri5<Eigen::VectorXd,
                         double,
                         Eigen::VectorXd,
                         double,
                         boost::numeric::odeint::vector_space_algebra>;

  auto saver = MetaSkeletonStateSpaceSaver(mMetaSkeletonStateSpace);
  DART_UNUSED(saver);

  mTimelimit = _timelimit;

  if (mMetaSkeleton->getPositionUpperLimits()
      == mMetaSkeleton->getPositionLowerLimits())
  {
    throw std::invalid_argument("State space volume zero");
  }
  if (mMetaSkeleton->getVelocityUpperLimits()
      == mMetaSkeleton->getVelocityLowerLimits())
  {
    throw std::invalid_argument("Velocity space volume zero");
  }

  for (std::size_t i = 0; i < mMetaSkeleton->getNumDofs(); ++i)
  {
    std::stringstream ss;
    if (mMetaSkeleton->getPositionLowerLimit(i)
        > mMetaSkeleton->getPositionUpperLimit(i))
    {
      ss << "Position lower limit is larger than upper limit at DOF " << i;
      throw std::invalid_argument(ss.str());
    }
    if (mMetaSkeleton->getVelocityLowerLimit(i)
        > mMetaSkeleton->getVelocityUpperLimit(i))
    {
      ss << "Velocity lower limit is larger than upper limit at DOF " << i;
      throw std::invalid_argument(ss.str());
    }
  }

  mKnots.clear();
  mCacheIndex = -1;
  mIndex = 0;

  mTimer.start();

  try
  {
    Eigen::VectorXd initialQ = mMetaSkeleton->getPositions();
    // Integrate the vector field to get a configuration space path.
    boost::numeric::odeint::integrate_adaptive(
        errorStepper(),
        std::bind(&VectorFieldPlanner::step, this, _1, _2, _3),
        initialQ,
        0.,
        _integrationTimeInterval,
        mInitialStepSize,
        std::bind(&VectorFieldPlanner::check, this, _1, _2));
  }
  catch (const VectorFieldTerminated& e)
  {
    dtwarn << e.what() << std::endl;
  }
  catch (const IntegrationFailedException& e)
  {
    dtwarn << e.what() << std::endl;
    return nullptr;
  }

  return convertToSpline(mKnots, mCacheIndex, mMetaSkeletonStateSpace);
}

//==============================================================================
std::unique_ptr<aikido::trajectory::Spline> planToEndEffectorOffset(
    const aikido::statespace::dart::MetaSkeletonStateSpacePtr& _stateSpace,
    const dart::dynamics::BodyNodePtr& _bn,
    const aikido::constraint::TestablePtr& _constraint,
    const Eigen::Vector3d& _direction,
    double _distance,
    double _maxDistance,
    double _positionTolerance,
    double _angularTolerance,
    double _timelimit,
    double _integralTimeInterval)
{
  if (_distance < 0.)
  {
    std::stringstream ss;
    ss << "Distance must be non-negative; got " << _distance << ".";
    throw std::runtime_error(ss.str());
  }

  if (_maxDistance < _distance)
  {
    throw std::runtime_error("Max distance is less than distance.");
  }

  if (_direction.norm() == 0.0)
  {
    throw std::runtime_error("Direction vector is a zero vector");
  }

  auto vectorfield = std::make_shared<MoveEndEffectorOffsetVectorField>(
      _stateSpace,
      _bn,
      _direction,
      _distance,
      _maxDistance,
      _positionTolerance,
      _angularTolerance);

  auto planner = std::make_shared<VectorFieldPlanner>(vectorfield, _constraint);
  return planner->followVectorField(_integralTimeInterval, _timelimit);
}

std::unique_ptr<aikido::trajectory::Spline> planToEndEffectorPose(
    const aikido::statespace::dart::MetaSkeletonStateSpacePtr& _stateSpace,
    const dart::dynamics::BodyNodePtr& _bn,
    const aikido::constraint::TestablePtr& _constraint,
    const Eigen::Isometry3d& _goalPose,
    double _poseErrorTolerance,
    double _timelimit,
    double _integralTimeInterval)
{
  auto vectorfield = std::make_shared<MoveEndEffectorPoseVectorField>(
      _stateSpace, _bn, _goalPose, _poseErrorTolerance);

  auto planner = std::make_shared<VectorFieldPlanner>(vectorfield, _constraint);
  return planner->followVectorField(_integralTimeInterval, _timelimit);
}

} // namespace vectorfield
} // namespace planner
} // namespace aikido