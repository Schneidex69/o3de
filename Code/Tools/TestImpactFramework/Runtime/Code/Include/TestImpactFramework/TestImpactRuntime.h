/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <TestImpactFramework/TestImpactConfiguration.h>
#include <TestImpactFramework/TestImpactChangeList.h>
#include <TestImpactFramework/TestImpactClientTestSelection.h>
#include <TestImpactFramework/TestImpactClientTestRun.h>
#include <TestImpactFramework/TestImpactClientFailureReport.h>
#include <TestImpactFramework/TestImpactTestSequence.h>

#include <AzCore/std/string/string.h>
#include <AzCore/std/chrono/chrono.h>
#include <AzCore/std/optional.h>
#include <AzCore/std/functional.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace TestImpact
{
    class ChangeDependencyList;
    class DynamicDependencyMap;
    class TestSelectorAndPrioritizer;
    class TestEngine;
    class TestTarget;
    class SourceCoveringTestsList;
    class TestEngineInstrumentedRun;

    //! Callback for a test sequence that isn't using test impact analysis to determine selected tests.
    //! @param tests The tests that will be run for this sequence.
    using TestSequenceStartCallback = AZStd::function<void(Client::TestRunSelection&& tests)>;

    //! Callback for a test sequence using test impact analysis.
    //! @param selectedTests The tests that have been selected for this run by test impact analysis.
    //! @param discardedTests The tests that have been rejected for this run by test impact analysis. 
    //! @param draftedTests The tests that have been drafted in for this run due to requirements outside of test impact analysis
    //! (e.g. test targets that have been added to the repository since the last test impact analysis sequence or test that failed
    //! to execute previously).
    //! These tests will be run with coverage instrumentation.
    //! @note discardedTests and draftedTests may contain overlapping tests.
    using ImpactAnalysisTestSequenceStartCallback = AZStd::function<void(
        Client::TestRunSelection&& selectedTests,
        AZStd::vector<AZStd::string>&& discardedTests,
        AZStd::vector<AZStd::string>&& draftedTests)>;

    //! Callback for a test sequence using test impact analysis.
    //! @param selectedTests The tests that have been selected for this run by test impact analysis.
    //! @param discardedTests The tests that have been rejected for this run by test impact analysis.
    //! These tests will not be run without coverage instrumentation unless there is an entry in the draftedTests list.
    //! @param draftedTests The tests that have been drafted in for this run due to requirements outside of test impact analysis
    //! (e.g. test targets that have been added to the repository since the last test impact analysis sequence or test that failed
    //! to execute previously).
    //! @note discardedTests and draftedTests may contain overlapping tests.
    using SafeImpactAnalysisTestSequenceStartCallback = AZStd::function<void(
        Client::TestRunSelection&& selectedTests,
        Client::TestRunSelection&& discardedTests,
        AZStd::vector<AZStd::string>&& draftedTests)>;

    //! Callback for end of a test sequence.
    //! @param failureReport The test runs that failed for any reason during this sequence.
    //! @param duration The total duration of this test sequence.
    using TestSequenceCompleteCallback = AZStd::function<void(
        Client::SequenceFailure&& failureReport,
        AZStd::chrono::milliseconds duration)>;

    //! Callback for end of a test impact analysis test sequence.
    //! @param selectedFailureReport The selected test runs that failed for any reason during this sequence.
    //! @param discardedFailureReport The discarded test runs that failed for any reason during this sequence.
    //! @param duration The total duration of this test sequence.
    using SafeTestSequenceCompleteCallback = AZStd::function<void(
        Client::SequenceFailure&& selectedFailureReport,
        Client::SequenceFailure&& discardedFailureReport,
        AZStd::chrono::milliseconds selectedDuration,
        AZStd::chrono::milliseconds discardedDuration)>;

    //! Callback for test runs that have completed for any reason.
    //! @param selectedTests The test that has completed.
    using TestRunCompleteCallback = AZStd::function<void(Client::TestRun&& selectedTests)>;

    //! The API exposed to the client responsible for all test runs and persistent data management.
    class Runtime
    {
    public:
        //! Constructs a runtime with the specified configuration and policies.
        //! @param config The configuration used for this runtime instance.
        //! @param suiteFilter The test suite for which the coverage data and test selection will draw from.
        //! @param executionFailurePolicy Determines how to handle test targets that fail to execute.
        //! @param executionFailureDraftingPolicy Determines how test targets that previously failed to execute are drafted into subsequent test sequences.
        //! @param testFailurePolicy Determines how to handle test targets that report test failures.
        //! @param integrationFailurePolicy Determines how to handle instances where the build system model and/or test impact analysis data is compromised.
        //! @param testShardingPolicy  Determines how to handle test targets that have opted in to test sharding.
        Runtime(
            RuntimeConfig&& config,
            SuiteType suiteFilter,
            Policy::ExecutionFailure executionFailurePolicy,
            Policy::FailedTestCoverage failedTestCoveragePolicy,
            Policy::TestFailure testFailurePolicy,
            Policy::IntegrityFailure integrationFailurePolicy,
            Policy::TestSharding testShardingPolicy,
            Policy::TargetOutputCapture targetOutputCapture,
            AZStd::optional<size_t> maxConcurrency = AZStd::nullopt);

        ~Runtime();

        //! Runs a test sequence where all tests with a matching suite in the suite filter and also not on the excluded list are selected.
        //! @param testTargetTimeout The maximum duration individual test targets may be in flight for (infinite if empty).
        //! @param globalTimeout The maximum duration the entire test sequence may run for (infinite if empty).
        //! @param testSequenceStartCallback The client function to be called after the test targets have been selected but prior to running the tests.
        //! @param testSequenceCompleteCallback The client function to be called after the test sequence has completed.
        //! @param testRunCompleteCallback The client function to be called after an individual test run has completed.
        //! @returns
        TestSequenceResult RegularTestSequence(
            AZStd::optional<AZStd::chrono::milliseconds> testTargetTimeout,
            AZStd::optional<AZStd::chrono::milliseconds> globalTimeout,
            AZStd::optional<TestSequenceStartCallback> testSequenceStartCallback,
            AZStd::optional<TestSequenceCompleteCallback> testSequenceCompleteCallback,
            AZStd::optional<TestRunCompleteCallback> testRunCompleteCallback);

        //! Runs a test sequence where tests are selected according to test impact analysis so long as they are not on the excluded list.
        //! @param changeList The change list used to determine the tests to select.
        //! @param testPrioritizationPolicy Determines how selected tests will be prioritized.
        //! @param dynamicDependencyMapPolicy The policy to determine how the coverage data of produced by test sequences is used to update the dynamic dependency map.
        //! @param testTargetTimeout The maximum duration individual test targets may be in flight for (infinite if empty).
        //! @param globalTimeout The maximum duration the entire test sequence may run for (infinite if empty).
        //! @param testSequenceStartCallback The client function to be called after the test targets have been selected but prior to running the tests.
        //! @param testSequenceCompleteCallback The client function to be called after the test sequence has completed.
        //! @param testRunCompleteCallback The client function to be called after an individual test run has completed.
        //! @returns
        TestSequenceResult ImpactAnalysisTestSequence(
            const ChangeList& changeList,
            Policy::TestPrioritization testPrioritizationPolicy,
            Policy::DynamicDependencyMap dynamicDependencyMapPolicy,
            AZStd::optional<AZStd::chrono::milliseconds> testTargetTimeout,
            AZStd::optional<AZStd::chrono::milliseconds> globalTimeout,
            AZStd::optional<ImpactAnalysisTestSequenceStartCallback> testSequenceStartCallback,
            AZStd::optional<TestSequenceCompleteCallback> testSequenceCompleteCallback,
            AZStd::optional<TestRunCompleteCallback> testRunCompleteCallback);

        //! Runs a test sequence as per the ImpactAnalysisTestSequence where the tests not selected are also run (albeit without instrumentation).
        //! @param changeList The change list used to determine the tests to select.
        //! @param testPrioritizationPolicy Determines how selected tests will be prioritized.
        //! @param testTargetTimeout The maximum duration individual test targets may be in flight for (infinite if empty).
        //! @param globalTimeout The maximum duration the entire test sequence may run for (infinite if empty).
        //! @param testSequenceStartCallback The client function to be called after the test targets have been selected but prior to running the tests.
        //! @param testSequenceCompleteCallback The client function to be called after the test sequence has completed.
        //! @param testRunCompleteCallback The client function to be called after an individual test run has completed.
        //! @returns
        AZStd::pair<TestSequenceResult, TestSequenceResult> SafeImpactAnalysisTestSequence(
            const ChangeList& changeList,
            Policy::TestPrioritization testPrioritizationPolicy,
            AZStd::optional<AZStd::chrono::milliseconds> testTargetTimeout,
            AZStd::optional<AZStd::chrono::milliseconds> globalTimeout,
            AZStd::optional<SafeImpactAnalysisTestSequenceStartCallback> testSequenceStartCallback,
            AZStd::optional<SafeTestSequenceCompleteCallback> testSequenceCompleteCallback,
            AZStd::optional<TestRunCompleteCallback> testRunCompleteCallback);

        //! Runs all tests not on the excluded list and uses their coverage data to seed the test impact analysis data (ant existing data will be overwritten).
        //! @param testTargetTimeout The maximum duration individual test targets may be in flight for (infinite if empty).
        //! @param globalTimeout The maximum duration the entire test sequence may run for (infinite if empty).
        //! @param testSequenceStartCallback The client function to be called after the test targets have been selected but prior to running the tests.
        //! @param testSequenceCompleteCallback The client function to be called after the test sequence has completed.
        //! @param testRunCompleteCallback The client function to be called after an individual test run has completed.
        //! 
        TestSequenceResult SeededTestSequence(
            AZStd::optional<AZStd::chrono::milliseconds> testTargetTimeout,
            AZStd::optional<AZStd::chrono::milliseconds> globalTimeout,
            AZStd::optional<TestSequenceStartCallback> testSequenceStartCallback,
            AZStd::optional<TestSequenceCompleteCallback> testSequenceCompleteCallback,
            AZStd::optional<TestRunCompleteCallback> testRunCompleteCallback);

        //! Returns true if the runtime has test impact analysis data (either preexisting or generated).
        bool HasImpactAnalysisData() const;

    private:
        //! Updates the test enumeration cache for test targets that had sources modified by a given change list.
        //! @param changeDependencyList The resolved change dependency list generated for the change list.
        void EnumerateMutatedTestTargets(const ChangeDependencyList& changeDependencyList);

        //! Selects the test targets covering a given change list and updates the enumeration cache of the test targets with sources
        //! modified in that change list.
        //! @param changeList The change list for which the covering tests and enumeration cache updates will be generated for.
        //! @param testPrioritizationPolicy The test prioritization strategy to use for the selected test targets.
        //! @returns The pair of selected test targets and discarded test targets.
        AZStd::pair<AZStd::vector<const TestTarget*>, AZStd::vector<const TestTarget*>> SelectCoveringTestTargetsAndUpdateEnumerationCache(
            const ChangeList& changeList,
            Policy::TestPrioritization testPrioritizationPolicy);

        //! Selects the test targets from the specified list of test targets that are not on the test target exclusion list.
        //! @param testTargets The list of test targets to select from.
        //! @returns The subset of test targets in the specified list that are not on the target exclude list.
        AZStd::pair<AZStd::vector<const TestTarget*>, AZStd::vector<const TestTarget*>> SelectTestTargetsByExcludeList(
            AZStd::vector<const TestTarget*> testTargets) const;

        //! Prunes the existing coverage for the specified jobs and creates the consolidated source covering tests list from the
        //! test engine instrumented run jobs.
        SourceCoveringTestsList CreateSourceCoveringTestFromTestCoverages(const AZStd::vector<TestEngineInstrumentedRun>& jobs);

        //! Prepares the dynamic dependency map for a seed update by clearing all existing data and deleting the file that will be serialized.
        void ClearDynamicDependencyMapAndRemoveExistingFile();

        //! Updates the dynamic dependency map and serializes the entire map to disk.
        void UpdateAndSerializeDynamicDependencyMap(const AZStd::vector<TestEngineInstrumentedRun>& jobs);

        RuntimeConfig m_config;
        SuiteType m_suiteFilter;
        RepoPath m_sparTIAFile;
        Policy::ExecutionFailure m_executionFailurePolicy;
        Policy::FailedTestCoverage m_failedTestCoveragePolicy;
        Policy::TestFailure m_testFailurePolicy;
        Policy::IntegrityFailure m_integrationFailurePolicy;
        Policy::TestSharding m_testShardingPolicy;
        Policy::TargetOutputCapture m_targetOutputCapture;
        size_t m_maxConcurrency = 0;
        AZStd::unique_ptr<DynamicDependencyMap> m_dynamicDependencyMap;
        AZStd::unique_ptr<TestSelectorAndPrioritizer> m_testSelectorAndPrioritizer;
        AZStd::unique_ptr<TestEngine> m_testEngine;
        AZStd::unordered_set<const TestTarget*> m_testTargetExcludeList;
        AZStd::unordered_set<const TestTarget*> m_testTargetShardList;
        bool m_hasImpactAnalysisData = false;
    };
} // namespace TestImpact
