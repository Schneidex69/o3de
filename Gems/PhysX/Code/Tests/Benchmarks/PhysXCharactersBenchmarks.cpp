/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <PhysX_precompiled.h>

#ifdef HAVE_BENCHMARK
#include <benchmark/benchmark.h>

#include <AzTest/AzTest.h>
#include <AzTest/Utils.h>

#include <AzCore/Math/MathUtils.h>
#include <AzCore/Math/Random.h>

#include <Benchmarks/PhysXBenchmarksCommon.h>
#include <Benchmarks/PhysXBenchmarksUtilities.h>

#include <AzFramework/Physics/SystemBus.h>

#include <PhysXCharacters/API/CharacterController.h>

#include <PhysXTestCommon.h>

namespace PhysX::Benchmarks
{
    namespace CharacterConstants
    {
        //! Controls the simulation length of the test. 30secs at 60fps
        static const int GameFramesToSimulate = 1800;

        //! The size of the test terrain
        static const float TerrainSize = 500.0f;

        //! Decide if CCD should be on/off for the following tests
        static const bool CCDEnabled = true;

        //! Constant seed to use with random number generation
        static const long long RandGenSeed = 909; //(Number generated by adding 'Character' ascii character codes together (67+104+97+114+97+99+116+101+114).

        //! Settings used to setup each benchmark
        namespace BenchmarkSettings
        {
            //! Values passed to benchmark to select the number of rigid bodies to spawn during each test
            //! Current values will run tests between StartRange to EndRange (inclusive), multiplying by RangeMultiplier each step.
            static const int StartRange = 1;
            static const int EndRange = 64;
            static const int RangeMultipler = 2;

            //! Number of iterations for each test
            static const int NumIterations = 3;
        } // namespace BenchmarkSettings

        //! Settings used for each character controller
        namespace CharacterSettings
        {
            //! Character controller configuration params
            static const float MaximumSlopeAngle = 25.0f;
            static const float StepHeight = 0.2f;

            //! Character box collider dimensions
            static const float CharacterBoxWidth = 0.5f;
            static const float CharacterBoxDepth = 0.5f;
            static const float CharacterBoxHeight = 1.0f;

            //! Character cylinder collider dimensions
            static const float CharacterCylinderHeight = 1.0f;
            static const float CharacterCylinderRadius = 0.25f;

            enum class ColliderType
            {
                Box = 0,
                Capsule
            };

            //! Max speed the character controller is able to move. Used when generating movement vectors
            static const float MaxCharacterSpeed = 5.0f;
        } // namespace CharacterSettings
    } // namespace CharacterConstants

    //! Character Controller performance fixture.
    //! Will create a world, and terrain used within the test.
    class PhysXCharactersBenchmarkFixture
        : public PhysX::Benchmarks::PhysXBaseBenchmarkFixture
    {
    public:
        virtual void SetUp([[maybe_unused]] const ::benchmark::State& state) override
        {
            PhysX::Benchmarks::PhysXBaseBenchmarkFixture::SetUpInternal();
            //need to get the Physics::System to be able to spawn the rigid bodies
            m_system = AZ::Interface<Physics::System>::Get();

            m_terrainEntity = PhysX::TestUtils::CreateFlatTestTerrain(m_testSceneHandle, CharacterConstants::TerrainSize, CharacterConstants::TerrainSize);
        }

        virtual void TearDown([[maybe_unused]] const ::benchmark::State& state) override
        {
            m_terrainEntity = nullptr;
            PhysX::Benchmarks::PhysXBaseBenchmarkFixture::TearDownInternal();
        }

    protected:
        // PhysXBaseBenchmarkFixture Overrides ...
        AzPhysics::SceneConfiguration GetDefaultSceneConfiguration() override
        {
            AzPhysics::SceneConfiguration sceneConfig = AzPhysics::SceneConfiguration::CreateDefault();
            sceneConfig.m_enableCcd = CharacterConstants::CCDEnabled;
            return sceneConfig;
        }

        Physics::System* m_system = nullptr;
        PhysX::EntityPtr m_terrainEntity = nullptr;
    };

    namespace Utils
    {
        //!Function pointers to allow customization of Utils::CreateCharacterControllers for specific tests
        using GenerateSpawnPositionFuncPtr = AZStd::function<const AZ::Vector3(int)>;

        //! Helper function to create the requested number of character controllers and where they spawn.
        //! @param numCharacterControllers, the number of character controllers to spawn
        //! @param colliderType, the collider type to use
        //! @param scene, the scene to spawn the characters controller into
        //! @param genSpawnPosFuncPtr - [optional] function pointer to allow caller to pick the spawn position
        AZStd::vector<Physics::Character*> CreateCharacterControllers(
            int numCharacterControllers,
            CharacterConstants::CharacterSettings::ColliderType colliderType,
            AzPhysics::SceneHandle& sceneHandle,
            GenerateSpawnPositionFuncPtr* genSpawnPosFuncPtr = nullptr)
        {
            //define some common configs
            PhysX::CharacterControllerConfiguration characterConfig;
            characterConfig.m_maximumSlopeAngle = CharacterConstants::CharacterSettings::MaximumSlopeAngle;
            characterConfig.m_stepHeight = CharacterConstants::CharacterSettings::StepHeight;

            switch (colliderType)
            {
            case CharacterConstants::CharacterSettings::ColliderType::Box:
                {
                    characterConfig.m_shapeConfig = AZStd::make_shared<Physics::BoxShapeConfiguration>(
                        AZ::Vector3(CharacterConstants::CharacterSettings::CharacterBoxWidth,
                            CharacterConstants::CharacterSettings::CharacterBoxDepth,
                            CharacterConstants::CharacterSettings::CharacterBoxHeight)
                    );

                }
                break;
            case CharacterConstants::CharacterSettings::ColliderType::Capsule:
            default:
                {
                    characterConfig.m_shapeConfig = AZStd::make_shared<Physics::CapsuleShapeConfiguration>(
                        CharacterConstants::CharacterSettings::CharacterCylinderHeight,
                        CharacterConstants::CharacterSettings::CharacterCylinderRadius);
                }
                break;
            }

            auto* sceneInterface = AZ::Interface<AzPhysics::SceneInterface>::Get();

            AZStd::vector<Physics::Character*> controllers;
            controllers.reserve(numCharacterControllers);
            for (int i = 0; i < numCharacterControllers; i++)
            {
                const AZ::Vector3 spawnPosition = genSpawnPosFuncPtr != nullptr ? (*genSpawnPosFuncPtr)(i) : AZ::Vector3::CreateZero();
                characterConfig.m_position = spawnPosition;
                AzPhysics::SimulatedBodyHandle newHandle = sceneInterface->AddSimulatedBody(sceneHandle, &characterConfig);
                if (newHandle != AzPhysics::InvalidSimulatedBodyHandle)
                {
                    if (auto* characterPtr = azdynamic_cast<Physics::Character*>(
                            sceneInterface->GetSimulatedBodyFromHandle(sceneHandle, newHandle)
                        ))
                    {
                        controllers.emplace_back(characterPtr);
                    }
                }
            }

            return controllers;
        }
    }

    //! BM_CharacterController_AtRest - This test just spawns the requested number of Character Controllers and places them near the terrain
    //! The test will run the simulation for ~1800 game frames at 60fps.
    BENCHMARK_DEFINE_F(PhysXCharactersBenchmarkFixture, BM_CharacterController_AtRest)(benchmark::State& state)
    {
        //setup some pieces for the test
        const int numCharacters = static_cast<const int>(state.range(0));

        //spawn character controllers
        const float spacing = (CharacterConstants::CharacterSettings::CharacterBoxWidth * 2);
        const int charactersPerCol = static_cast<const int>(CharacterConstants::TerrainSize / spacing) - 1;
        int spawnColIdx = 0;
        int spawnRowIdx = 0;
        Utils::GenerateSpawnPositionFuncPtr posGenerator = [spacing, charactersPerCol, &spawnColIdx, &spawnRowIdx]([[maybe_unused]] int idx) -> const AZ::Vector3 {
            const float x = spacing + (spacing * spawnColIdx);
            const float y = spacing + (spacing * spawnRowIdx);
            const float z = 0.0f;

            //advance to the next position to spawn the next rigid body
            spawnColIdx++;
            if (spawnColIdx >= charactersPerCol)
            {
                spawnColIdx = 0;
                spawnRowIdx++;
            }
            return AZ::Vector3(x, y, z);
        };
        AZStd::vector<Physics::Character*> controllers = Utils::CreateCharacterControllers(numCharacters,
            static_cast<CharacterConstants::CharacterSettings::ColliderType>(state.range(1)), m_testSceneHandle, &posGenerator);
        
        //setup the sub tick tracker
        PhysX::Benchmarks::Utils::PrePostSimulationEventHandler subTickTracker;
        subTickTracker.Start(m_defaultScene);

        //setup the frame timer tracker
        AZStd::vector<double> tickTimes;
        tickTimes.reserve(CharacterConstants::GameFramesToSimulate);
        for (auto _ : state)
        {
            for (AZ::u32 i = 0; i < CharacterConstants::GameFramesToSimulate; i++)
            {
                auto start = AZStd::chrono::system_clock::now();
                StepScene1Tick(DefaultTimeStep);

                //time each physics tick and store it to analyze
                auto tickElapsedMilliseconds = PhysX::Benchmarks::Types::double_milliseconds(AZStd::chrono::system_clock::now() - start);
                tickTimes.emplace_back(tickElapsedMilliseconds.count());
            }
        }
        subTickTracker.Stop();

        //get the P50, P90, P99 percentiles
        PhysX::Benchmarks::Utils::ReportFramePercentileCounters(state, tickTimes, subTickTracker.GetSubTickTimes());
        PhysX::Benchmarks::Utils::ReportFrameStandardDeviationAndMeanCounters(state, tickTimes, subTickTracker.GetSubTickTimes());
    }

    //! BM_CharacterController_Moving_StraightLine - This test just spawns the requested number of Character Controllers and places them near the terrain
    //! The test will then start marking the characters is one direction
    //! The test will run the simulation for ~1800 game frames at 60fps.
    BENCHMARK_DEFINE_F(PhysXCharactersBenchmarkFixture, BM_CharacterController_Moving_StraightLine)(benchmark::State& state)
    {
        //setup some pieces for the test
        const int numCharacters = static_cast<const int>(state.range(0));

        //spawn character controllers
        const float spacing = (CharacterConstants::CharacterSettings::CharacterBoxWidth * 2);
        const int charactersPerCol = static_cast<const int>(CharacterConstants::TerrainSize / spacing) - 1;
        int spawnColIdx = 0;
        int spawnRowIdx = 0;
        Utils::GenerateSpawnPositionFuncPtr posGenerator = [spacing, charactersPerCol, &spawnColIdx, &spawnRowIdx]([[maybe_unused]] int idx) -> const AZ::Vector3 {
            const float x = spacing + (spacing * spawnColIdx);
            const float y = spacing + (spacing * spawnRowIdx);
            const float z = 0.0f;

            //advance to the next position to spawn the next rigid body
            spawnColIdx++;
            if (spawnColIdx >= charactersPerCol)
            {
                spawnColIdx = 0;
                spawnRowIdx++;
            }
            return AZ::Vector3(x, y, z);
        };
        AZStd::vector<Physics::Character*> controllers = Utils::CreateCharacterControllers(numCharacters,
            static_cast<CharacterConstants::CharacterSettings::ColliderType>(state.range(1)), m_testSceneHandle, &posGenerator);

        //setup the sub tick tracker
        PhysX::Benchmarks::Utils::PrePostSimulationEventHandler subTickTracker;
        subTickTracker.Start(m_defaultScene);

        //Setup all characters movement
        AZ::Vector3 movementVelocity(0.0f, 1.0f, 0.0f);
        //setup the frame timer tracker

        AZStd::vector<double> tickTimes;
        tickTimes.reserve(CharacterConstants::GameFramesToSimulate);
        for (auto _ : state)
        {
            for (AZ::u32 i = 0; i < CharacterConstants::GameFramesToSimulate; i++)
            {
                auto start = AZStd::chrono::system_clock::now();
                //update the movement of all the characters controllers
                for (auto& controller : controllers)
                {
                    controller->AddVelocity(movementVelocity);
                    controller->ApplyRequestedVelocity(PhysX::Benchmarks::DefaultTimeStep);
                }

                StepScene1Tick(DefaultTimeStep);

                //time each physics tick and store it to analyze
                auto tickElapsedMilliseconds = PhysX::Benchmarks::Types::double_milliseconds(AZStd::chrono::system_clock::now() - start);
                tickTimes.emplace_back(tickElapsedMilliseconds.count());
            }
        }
        subTickTracker.Stop();

        //get the P50, P90, P99 percentiles
        PhysX::Benchmarks::Utils::ReportFramePercentileCounters(state, tickTimes, subTickTracker.GetSubTickTimes());
        PhysX::Benchmarks::Utils::ReportFrameStandardDeviationAndMeanCounters(state, tickTimes, subTickTracker.GetSubTickTimes());
    }

    //! BM_CharacterController_Moving_Randomly - This test just spawns the requested number of Character Controllers and places them near the terrain
    //! The test will then start making the characters move in random directions
    //! The test will run the simulation for ~1800 game frames at 60fps.
    BENCHMARK_DEFINE_F(PhysXCharactersBenchmarkFixture, BM_CharacterController_Moving_Randomly)(benchmark::State& state)
    {
        //setup some pieces for the test
        AZ::SimpleLcgRandom rand;
        rand.SetSeed(CharacterConstants::RandGenSeed);
        const int numCharacters = static_cast<const int>(state.range(0));

        //spawn character controllers
        const float spawnAreaSize = CharacterConstants::TerrainSize * 0.25f;
        const float spawnAreaCenter = CharacterConstants::TerrainSize * 0.5f;
        Utils::GenerateSpawnPositionFuncPtr posGenerator = [spawnAreaSize, spawnAreaCenter, &rand]([[maybe_unused]] int idx) -> const AZ::Vector3 {
            const float x = spawnAreaCenter + rand.GetRandomFloat() * spawnAreaSize;
            const float y = spawnAreaCenter + rand.GetRandomFloat() * spawnAreaSize;
            const float z = 0.0f;
            return AZ::Vector3(x, y, z);
        };
        AZStd::vector<Physics::Character*> controllers = Utils::CreateCharacterControllers(numCharacters,
            static_cast<CharacterConstants::CharacterSettings::ColliderType>(state.range(1)), m_testSceneHandle, &posGenerator);

        //pair up each character controller with a movement vector
        using ControllerAndMovementDirPair = AZStd::pair<Physics::Character*, AZ::Vector3>;
        AZStd::vector<ControllerAndMovementDirPair> targetMoveAndControllers;
        for (auto& controller : controllers)
        {
            targetMoveAndControllers.emplace_back(ControllerAndMovementDirPair(controller, AZ::Vector3::CreateZero()));
        }

        //setup the sub tick tracker
        PhysX::Benchmarks::Utils::PrePostSimulationEventHandler subTickTracker;
        subTickTracker.Start(m_defaultScene);

        //break the sim into parts, and change direction each time
        const AZ::u32 numDirectionChanges = 20;
        const AZ::u32 numFramesPreDirection = CharacterConstants::GameFramesToSimulate / numDirectionChanges;

        //setup the frame timer tracker
        AZStd::vector<double> tickTimes;
        tickTimes.reserve(CharacterConstants::GameFramesToSimulate);
        for (auto _ : state)
        {
            //run each simulation part, and change direction each time
            for (AZ::u32 i = 0; i < numDirectionChanges; i++)
            {
                //Setup all characters movement - this section is not timed
                for (auto& controllerMovementPair : targetMoveAndControllers)
                {
                    //convert from 0..1 to -1..1
                    float x = ((rand.GetRandomFloat() * 2.0f - 1.0f) * CharacterConstants::CharacterSettings::MaxCharacterSpeed);
                    float y = ((rand.GetRandomFloat() * 2.0f - 1.0f) * CharacterConstants::CharacterSettings::MaxCharacterSpeed);
                    controllerMovementPair.second = AZ::Vector3(x, y, 0.0f);
                }

                for (AZ::u32 j = 0; j < numFramesPreDirection; j++)
                {
                    auto start = AZStd::chrono::system_clock::now();
                    //update the movement of all the characters controllers
                    for (auto& controllerMovementPair : targetMoveAndControllers)
                    {
                        controllerMovementPair.first->AddVelocity(controllerMovementPair.second);
                        controllerMovementPair.first->ApplyRequestedVelocity(PhysX::Benchmarks::DefaultTimeStep);
                    }

                    StepScene1Tick(DefaultTimeStep);

                    //time each physics tick and store it to analyze
                    auto tickElapsedMilliseconds = PhysX::Benchmarks::Types::double_milliseconds(AZStd::chrono::system_clock::now() - start);
                    tickTimes.emplace_back(tickElapsedMilliseconds.count());
                }
            }
        }
        subTickTracker.Stop();

        //get the P50, P90, P99 percentiles
        PhysX::Benchmarks::Utils::ReportFramePercentileCounters(state, tickTimes, subTickTracker.GetSubTickTimes());
        PhysX::Benchmarks::Utils::ReportFrameStandardDeviationAndMeanCounters(state, tickTimes, subTickTracker.GetSubTickTimes());
    }

    BENCHMARK_REGISTER_F(PhysXCharactersBenchmarkFixture, BM_CharacterController_AtRest)
        ->RangeMultiplier(CharacterConstants::BenchmarkSettings::RangeMultipler)
        ->Ranges({
            {CharacterConstants::BenchmarkSettings::StartRange, CharacterConstants::BenchmarkSettings::EndRange},
            {static_cast<int>(CharacterConstants::CharacterSettings::ColliderType::Box), static_cast<int>(CharacterConstants::CharacterSettings::ColliderType::Capsule)}
            })
        ->Unit(benchmark::kMillisecond)
        ->Iterations(CharacterConstants::BenchmarkSettings::NumIterations)
        ;

    BENCHMARK_REGISTER_F(PhysXCharactersBenchmarkFixture, BM_CharacterController_Moving_StraightLine)
        ->RangeMultiplier(CharacterConstants::BenchmarkSettings::RangeMultipler)
        ->Ranges({
            {CharacterConstants::BenchmarkSettings::StartRange, CharacterConstants::BenchmarkSettings::EndRange},
            {static_cast<int>(CharacterConstants::CharacterSettings::ColliderType::Box), static_cast<int>(CharacterConstants::CharacterSettings::ColliderType::Capsule)}
            })
        ->Unit(benchmark::kMillisecond)
        ->Iterations(CharacterConstants::BenchmarkSettings::NumIterations)
        ;

    BENCHMARK_REGISTER_F(PhysXCharactersBenchmarkFixture, BM_CharacterController_Moving_Randomly)
        ->RangeMultiplier(CharacterConstants::BenchmarkSettings::RangeMultipler)
        ->Ranges({
            {CharacterConstants::BenchmarkSettings::StartRange, CharacterConstants::BenchmarkSettings::EndRange},
            {static_cast<int>(CharacterConstants::CharacterSettings::ColliderType::Box), static_cast<int>(CharacterConstants::CharacterSettings::ColliderType::Capsule)}
            })
        ->Unit(benchmark::kMillisecond)
        ->Iterations(CharacterConstants::BenchmarkSettings::NumIterations)
        ;

} // namespace PhysX::Benchmarks

#endif // #ifdef HAVE_BENCHMARK
