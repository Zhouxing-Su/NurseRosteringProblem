using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization;
using System.Threading;


namespace INRC2
{
    using NurseRostering;


    [DataContract]
    public class Misson
    {
        /// <summary> 
        /// max number of running threads at any time. 
        /// 0 for making use of all logical CPUs.
        /// </summary>
        [DataMember]
        public int concurrency;

        /// <summary>
        /// number of runs on each sequence. 
        /// 0 for keep running until manually stopped.
        /// </summary>
        [DataMember]
        public int repeat;

        /// <summary> strings in format of "scenario initHistory weekdataSequence". </summary>
        /// <example> {"n030w8 0 12345678", "n120w4 2 6666"}. </example>
        [DataMember]
        public string[] sequences;

        /// <summary> key: nurse number. value: time for single stage in seconds. </summary>
        [DataMember]
        public Dictionary<int, double> timeouts;
    }

    public class Simulator
    {
        #region Constructor
        public Simulator(Misson mission) {
            this.mission = mission;
        }
        #endregion Constructor

        #region Method
        /// <summary>
        /// if ($repeat > 0), the order of tasks will be scheduled by running time. 
        /// the threads will run in foreground so the application will not exit 
        /// until all tasks are finished. it will block until all tasks start.
        /// </summary>
        public void launch() {
            Directory.CreateDirectory(DefaultOutputDir);
            if (!File.Exists(DefaultLogPath)) {
                File.WriteAllText(DefaultLogPath,
                    "Time,ID,Instance,Config,RandSeed,GenCount,IterCount,Duration,Feasible,Check-Obj,ObjValue,AccObjValue,Solution");
            }

            using (SemaphoreSlim availableThread = new SemaphoreSlim((mission.concurrency == 0)
                ? Environment.ProcessorCount : mission.concurrency)) {
                if (mission.repeat > 0) {
                    Task<string>[] tasks = new Task<string>[mission.repeat * mission.sequences.Length];
                    for (int i = 0, k = 0; i < mission.repeat; i++) {
                        for (int j = 0; j < mission.sequences.Length; j++) {
                            int nurseNum = JsonData.getNurseNumber(mission.sequences[i]);
                            int weekCount = JsonData.getWeekCount(mission.sequences[i]);
                            tasks[k++] = new Task<string>(runInstanceWithCustomFile,
                                mission.sequences[j], (int)(mission.timeouts[nurseNum] * weekCount));
                        }
                    }

                    // sort to let task cost more time comes earlier
                    Array.Sort(tasks, (lhs, rhs) => (rhs.duration - lhs.duration));

                    foreach (Task<string> task in tasks) {
                        availableThread.Wait();
                        new Thread((arg) => {
                            task.action((string)arg);
                            availableThread.Release();
                        }).Start(task.arg);
                    }
                } else {
                    while (true) {
                        foreach (string seq in mission.sequences) {
                            availableThread.Wait();
                            new Thread((arg) => {
                                runInstanceWithCustomFile((string)arg);
                                availableThread.Release();
                            }).Start(seq);
                        }
                    }
                }
            }
        }

        public void runInstanceWithCustomFile(string sequence) {
            string[] strs = sequence.Split('|');
            string scenario = strs[0];
            char initHistory = strs[1][0];
            string weekdataSeq = strs[2];
            int nurseNum = JsonData.getNurseNumber(scenario);

            string[] args = (string[])Args.Clone();
            Solver.Environment env = new Solver.Environment();
            env.scenarioPath = JsonData.getScenarioFilePath(scenario);
            //env.id = "0";                         // leave it as default value
            //env.randSeed = 0;                     // leave it as default value
            //env.maxIterCount = IterCounts.Max;    // leave it as default value
            env.timeoutInSeconds = mission.timeouts[nurseNum];

            int week = 0;
            string outputPathPrefix = DefaultOutputDir + env.id + "." + week;
            string environmentPath = outputPathPrefix + DefaultEnvironmentPath;
            env.historyPath = JsonData.getInitHistoryFilePath(scenario, initHistory);
            env.customOutputPath = outputPathPrefix + DefaultCustomFilePath;
            env.weekdataPath = JsonData.getWeekdataFilePath(scenario, weekdataSeq[week]);
            env.solutionPath = outputPathPrefix + DefaultSolutionPath;
            env.instanceInfo = sequence + "@" + week;

            Util.writeJsonFile(environmentPath, env);
            args[1] = environmentPath;
            NurseRostering.Program.Main(args);

            env.historyPath = null;
            while (++week < weekdataSeq.Length) {
                outputPathPrefix = DefaultOutputDir + env.id + "." + week;
                env.customInputPath = env.customOutputPath;
                env.customOutputPath = outputPathPrefix + DefaultCustomFilePath;
                env.weekdataPath = JsonData.getWeekdataFilePath(scenario, weekdataSeq[week]);
                env.solutionPath = outputPathPrefix + DefaultSolutionPath;
                env.instanceInfo = sequence + "@" + week;

                environmentPath = outputPathPrefix + DefaultEnvironmentPath;
                Util.writeJsonFile(environmentPath, env);
                args[1] = environmentPath;
                NurseRostering.Program.Main(args);
            }
        }

        public void runInstanceWithHistory(string sequence) {
            string[] strs = sequence.Split('|');
            string scenario = strs[0];
            char initHistory = strs[1][0];
            string weekdataSeq = strs[2];
            int nurseNum = JsonData.getNurseNumber(scenario);

            string[] args = (string[])Args.Clone();
            Solver.Environment env = new Solver.Environment();
            env.scenarioPath = JsonData.getScenarioFilePath(scenario);
            //env.id = "0";                         // leave it as default value
            //env.randSeed = 0;                     // leave it as default value
            //env.maxIterCount = IterCounts.Max;    // leave it as default value
            env.timeoutInSeconds = mission.timeouts[nurseNum];

            int week = 0;
            string outputPathPrefix = DefaultOutputDir + env.id + "." + week;
            string environmentPath = outputPathPrefix + DefaultEnvironmentPath;

            env.historyPath = JsonData.getInitHistoryFilePath(scenario, initHistory);
            env.weekdataPath = JsonData.getWeekdataFilePath(scenario, weekdataSeq[week]);
            env.solutionPath = outputPathPrefix + DefaultSolutionPath;
            env.instanceInfo = sequence + "[" + week + "]";

            Util.writeJsonFile(environmentPath, env);
            args[1] = environmentPath;
            NurseRostering.Program.Main(args);

            while (++week < weekdataSeq.Length) {
                outputPathPrefix = DefaultOutputDir + env.id + "." + week;
                environmentPath = outputPathPrefix + DefaultEnvironmentPath;

                generateHistory(env, outputPathPrefix);
                env.historyPath = outputPathPrefix + DefaultHistoryPath;
                env.weekdataPath = JsonData.getWeekdataFilePath(scenario, weekdataSeq[week]);
                env.solutionPath = outputPathPrefix + DefaultSolutionPath;
                env.instanceInfo = sequence + "[" + week + "]";

                Util.writeJsonFile(environmentPath, env);
                args[1] = environmentPath;
                NurseRostering.Program.Main(args);
            }
        }

        private void generateHistory(Solver.Environment env, string newHistoryPathPrefix) {
            // UNDONE[6]: simulator generate history from solution

        }
        #endregion Method

        #region Property
        #endregion Property

        #region Type
        private class Task<T>
        {
            public Task(Action<T> action, T arg, int duration) {
                this.action = action;
                this.arg = arg;
                this.duration = duration;
            }

            public Action<T> action;
            public T arg;
            public int duration;
        }
        #endregion Type

        #region Constant
        public const string DefaultMissionPath = "mission.json";

        public const string DefaultOutputDir = "Output/";
        public const string DefaultSolutionPath = "solution.json";
        public const string DefaultHistoryPath = "history.json";
        public const string DefaultCustomFilePath = "custom.json";

        public const string DefaultEnvironmentPath = "environment.json";
        public const string DefaultConfigPath = "configure.json";
        public const string DefaultLogPath = "log.csv";

        public readonly string[] Args = new string[]{
            NurseRostering.Program.Parameters.EnvironmentPathOption, null,
            NurseRostering.Program.Parameters.ConfigurePathOption, DefaultConfigPath,
            NurseRostering.Program.Parameters.LogPathOption, DefaultLogPath
        };
        #endregion Constant

        #region Field
        private Misson mission;
        #endregion Field
    }
}
