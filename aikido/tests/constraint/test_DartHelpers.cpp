#include <gtest/gtest.h>
#include <dart/dynamics/dynamics.h>
#include <dart/common/StlHelpers.h>
#include <aikido/constraint/dart.hpp>
#include <aikido/constraint/SatisfiedConstraint.hpp>
#include <aikido/statespace/dart/RnJoint.hpp>
#include <aikido/statespace/dart/SO2Joint.hpp>
#include <aikido/statespace/dart/SO3Joint.hpp>

using Vector0d = Eigen::Matrix<double, 0, 1>;
using Vector1d = Eigen::Matrix<double, 1, 1>;

using dart::common::make_unique;
using dart::dynamics::BallJoint;
using dart::dynamics::BodyNode;
using dart::dynamics::Joint;
using dart::dynamics::RevoluteJoint;
using dart::dynamics::Skeleton;
using dart::dynamics::SkeletonPtr;
using aikido::constraint::SampleGenerator;
using aikido::constraint::SatisfiedConstraint;
using aikido::statespace::dart::RnJoint;
using aikido::statespace::dart::SO2Joint;
using aikido::statespace::dart::SO3Joint;
using aikido::util::RNGWrapper;

using aikido::constraint::createDifferentiableBoundsFor;
using aikido::constraint::createProjectableBoundsFor;
using aikido::constraint::createTestableBoundsFor;
using aikido::constraint::createSampleableBoundsFor;

static Vector1d make_vector(double _x)
{
  Vector1d matrix;
  matrix(0, 0) = _x;
  return matrix;
}

//=============================================================================
class RnJointHelpersTests : public ::testing::Test
{
protected:
  static constexpr int NUM_SAMPLES { 1000 };

  void SetUp() override
  {
    mSkeleton = Skeleton::create();
    mJoint = mSkeleton->createJointAndBodyNodePair<
      RevoluteJoint, BodyNode>().first;
    mJoint->setPositionLowerLimit(0, -1.);
    mJoint->setPositionUpperLimit(0, 2.);

    mStateSpace = std::make_shared<RnJoint>(mJoint);
  }

  SkeletonPtr mSkeleton;
  RevoluteJoint* mJoint;
  std::shared_ptr<RnJoint> mStateSpace;
};

//=============================================================================
TEST_F(RnJointHelpersTests, createTestableBoundsFor)
{
  auto constraint = createTestableBoundsFor<RnJoint>(
    mStateSpace);
  auto state = mStateSpace->createState();

  EXPECT_EQ(mStateSpace, constraint->getStateSpace());

  mJoint->setPosition(0, -0.9);
  mStateSpace->getState(state);
  EXPECT_TRUE(constraint->isSatisfied(state));

  mJoint->setPosition(0, 1.9);
  mStateSpace->getState(state);
  EXPECT_TRUE(constraint->isSatisfied(state));

  mJoint->setPosition(0, -1.1);
  mStateSpace->getState(state);
  EXPECT_FALSE(constraint->isSatisfied(state));

  mJoint->setPosition(0, 2.1);
  mStateSpace->getState(state);
  EXPECT_FALSE(constraint->isSatisfied(state));
}

//=============================================================================
TEST_F(RnJointHelpersTests, createProjectableBounds)
{
  auto testableConstraint
    = createTestableBoundsFor<RnJoint>(mStateSpace);
  auto projectableConstraint
    = createProjectableBoundsFor<RnJoint>(mStateSpace);

  auto inState = mStateSpace->createState();
  auto outState = mStateSpace->createState();

  EXPECT_EQ(mStateSpace, projectableConstraint->getStateSpace());

  // Doesn't change the value if the constraint is satisfied.
  mJoint->setPosition(0, -0.9);
  mStateSpace->getState(inState);
  EXPECT_TRUE(projectableConstraint->project(inState, outState));
  EXPECT_TRUE(inState.getValue().isApprox(outState.getValue()));

  mJoint->setPosition(0, 1.9);
  mStateSpace->getState(inState);
  EXPECT_TRUE(projectableConstraint->project(inState, outState));
  EXPECT_TRUE(inState.getValue().isApprox(outState.getValue()));

  // Output is feasible if the constriant is not satisfied.
  mJoint->setPosition(0, -1.1);
  mStateSpace->getState(inState);
  EXPECT_TRUE(projectableConstraint->project(inState, outState));
  EXPECT_TRUE(testableConstraint->isSatisfied(outState));

  mJoint->setPosition(0, 2.1);
  mStateSpace->getState(inState);
  EXPECT_TRUE(projectableConstraint->project(inState, outState));
  EXPECT_TRUE(testableConstraint->isSatisfied(outState));
}

//=============================================================================
TEST_F(RnJointHelpersTests, createDifferentiableBounds)
{
  const auto differentiableConstraint
    = createDifferentiableBoundsFor<RnJoint>(mStateSpace);

  EXPECT_EQ(mStateSpace, differentiableConstraint->getStateSpace());

  auto state = mStateSpace->createState();

  // Value is zero when the constraint is satisfied.
  mJoint->setPosition(0, -0.9);
  mStateSpace->getState(state);
  auto constraintValue = differentiableConstraint->getValue(state);
  EXPECT_TRUE(Vector1d::Zero().isApprox(constraintValue));

  mJoint->setPosition(0, 1.9);
  mStateSpace->getState(state);
  constraintValue = differentiableConstraint->getValue(state);
  EXPECT_TRUE(Vector1d::Zero().isApprox(constraintValue));

  // Value is non-zero when the constraint is not satisfied.
  mJoint->setPosition(0, -1.1);
  mStateSpace->getState(state);
  constraintValue = differentiableConstraint->getValue(state);
  EXPECT_FALSE(Vector1d::Zero().isApprox(constraintValue));

  mJoint->setPosition(0, 2.1);
  mStateSpace->getState(state);
  constraintValue = differentiableConstraint->getValue(state);
  EXPECT_FALSE(Vector1d::Zero().isApprox(constraintValue));
}

//=============================================================================
TEST_F(RnJointHelpersTests, createSampleableBounds)
{
  auto rng = make_unique<RNGWrapper<std::default_random_engine>>(0);
  const auto testableConstraint
    = createTestableBoundsFor<RnJoint>(mStateSpace);
  const auto sampleableConstraint
    = createSampleableBoundsFor<RnJoint>(
        mStateSpace, std::move(rng));
  ASSERT_TRUE(!!sampleableConstraint);
  EXPECT_EQ(mStateSpace, sampleableConstraint->getStateSpace());

  const auto generator = sampleableConstraint->createSampleGenerator();
  ASSERT_TRUE(!!generator);
  EXPECT_EQ(mStateSpace, generator->getStateSpace());

  auto state = mStateSpace->createState();

  for (size_t isample = 0; isample < NUM_SAMPLES; ++isample)
  {
    ASSERT_TRUE(generator->canSample());
    ASSERT_EQ(SampleGenerator::NO_LIMIT, generator->getNumSamples());
    ASSERT_TRUE(generator->sample(state));
    ASSERT_TRUE(testableConstraint->isSatisfied(state));
  }
}

//=============================================================================
class SO2JointHelpersTests : public ::testing::Test
{
protected:
  static constexpr int NUM_SAMPLES { 1000 };

  void SetUp() override
  {
    mSkeleton = Skeleton::create();
    mJoint = mSkeleton->createJointAndBodyNodePair<
      RevoluteJoint, BodyNode>().first;
    // Don't set any limits.

    mStateSpace = std::make_shared<SO2Joint>(mJoint);
  }

  SkeletonPtr mSkeleton;
  RevoluteJoint* mJoint;
  std::shared_ptr<SO2Joint> mStateSpace;
};

//=============================================================================
TEST_F(SO2JointHelpersTests, createTestableBoundsFor)
{
  const auto constraint
    = createTestableBoundsFor<SO2Joint>(mStateSpace);
  EXPECT_TRUE(!!dynamic_cast<SatisfiedConstraint*>(constraint.get()));
  EXPECT_EQ(mStateSpace, constraint->getStateSpace());
}

//=============================================================================
TEST_F(SO2JointHelpersTests, createProjectableBounds)
{
  const auto constraint
    = createProjectableBoundsFor<SO2Joint>(mStateSpace);
  EXPECT_TRUE(!!dynamic_cast<SatisfiedConstraint*>(constraint.get()));
  EXPECT_EQ(mStateSpace, constraint->getStateSpace());
}

//=============================================================================
TEST_F(SO2JointHelpersTests, createDifferentiableBounds)
{
  const auto constraint
    = createDifferentiableBoundsFor<SO2Joint>(mStateSpace);
  EXPECT_TRUE(!!dynamic_cast<SatisfiedConstraint*>(constraint.get()));
  EXPECT_EQ(mStateSpace, constraint->getStateSpace());
}

//=============================================================================
TEST_F(SO2JointHelpersTests, createSampleableBounds)
{
  const auto sampleableConstraint
    = createSampleableBoundsFor<SO2Joint>(mStateSpace,
        make_unique<RNGWrapper<std::default_random_engine>>(0));

  ASSERT_TRUE(!!sampleableConstraint);
  EXPECT_EQ(mStateSpace, sampleableConstraint->getStateSpace());

  const auto generator = sampleableConstraint->createSampleGenerator();
  ASSERT_TRUE(!!generator);
  EXPECT_EQ(mStateSpace, generator->getStateSpace());

  auto state = mStateSpace->createState();

  for (size_t isample = 0; isample < NUM_SAMPLES; ++isample)
  {
    ASSERT_TRUE(generator->canSample());
    ASSERT_EQ(SampleGenerator::NO_LIMIT, generator->getNumSamples());
    ASSERT_TRUE(generator->sample(state));
    // There is nothing to test here.
  }
}

//=============================================================================
class SO3JointHelpersTests : public ::testing::Test
{
protected:
  static constexpr int NUM_SAMPLES { 1000 };

  void SetUp() override
  {
    mSkeleton = Skeleton::create();
    mJoint = mSkeleton->createJointAndBodyNodePair<
      BallJoint, BodyNode>().first;
    // Don't set any limits.

    mStateSpace = std::make_shared<SO3Joint>(mJoint);
  }

  SkeletonPtr mSkeleton;
  BallJoint* mJoint;
  std::shared_ptr<SO3Joint> mStateSpace;
};

//=============================================================================
TEST_F(SO3JointHelpersTests, createTestableBoundsFor)
{
  const auto constraint
    = createTestableBoundsFor<SO3Joint>(mStateSpace);
  EXPECT_TRUE(!!dynamic_cast<SatisfiedConstraint*>(constraint.get()));
  EXPECT_EQ(mStateSpace, constraint->getStateSpace());
}

//=============================================================================
TEST_F(SO3JointHelpersTests, createProjectableBounds)
{
  const auto constraint
    = createProjectableBoundsFor<SO3Joint>(mStateSpace);
  EXPECT_TRUE(!!dynamic_cast<SatisfiedConstraint*>(constraint.get()));
  EXPECT_EQ(mStateSpace, constraint->getStateSpace());
}

//=============================================================================
TEST_F(SO3JointHelpersTests, createDifferentiableBounds)
{
  const auto constraint
    = createDifferentiableBoundsFor<SO3Joint>(mStateSpace);
  EXPECT_TRUE(!!dynamic_cast<SatisfiedConstraint*>(constraint.get()));
  EXPECT_EQ(mStateSpace, constraint->getStateSpace());
}

//=============================================================================
TEST_F(SO3JointHelpersTests, createSampleableBounds)
{
  const auto sampleableConstraint
    = createSampleableBoundsFor<SO3Joint>(mStateSpace,
        make_unique<RNGWrapper<std::default_random_engine>>(0));

  ASSERT_TRUE(!!sampleableConstraint);
  EXPECT_EQ(mStateSpace, sampleableConstraint->getStateSpace());

  const auto generator = sampleableConstraint->createSampleGenerator();
  ASSERT_TRUE(!!generator);
  EXPECT_EQ(mStateSpace, generator->getStateSpace());

  auto state = mStateSpace->createState();

  for (size_t isample = 0; isample < NUM_SAMPLES; ++isample)
  {
    ASSERT_TRUE(generator->canSample());
    ASSERT_EQ(SampleGenerator::NO_LIMIT, generator->getNumSamples());
    ASSERT_TRUE(generator->sample(state));
    // There is nothing to test here.
  }
}

// TODO: Add tests for SE2 bounds once they're implemented.
// TODO: Add tests for SE3 bounds once they're implemented.
